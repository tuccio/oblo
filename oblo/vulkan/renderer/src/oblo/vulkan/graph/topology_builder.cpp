#include <oblo/vulkan/graph/topology_builder.hpp>

#include <oblo/core/utility.hpp>

namespace oblo::vk
{
    namespace
    {
        u32 read_u32(const void* ptr, u32 offset)
        {
            u32 result;
            std::memcpy(&result, static_cast<const u8*>(ptr) + offset, sizeof(u32));
            return result;
        }

        void write_u32(void* ptr, u32 offset, u32 value)
        {
            std::memcpy(static_cast<u8*>(ptr) + offset, &value, sizeof(u32));
        }
    }

    expected<render_graph, graph_error> topology_builder::build()
    {
        enum class visit_state : u8
        {
            unvisited,
            visiting,
            visited
        };

        const usize numNodes = m_nodes.size();

        std::vector<visit_state> nodeStates{numNodes, visit_state::unvisited};

        std::vector<usize> nodesOrder;
        nodesOrder.reserve(numNodes);

        using node_entry_ptr = decltype(m_nodes)::value_type*;
        std::vector<node_entry_ptr> linearizedNodes;

        linearizedNodes.reserve(m_nodes.size());

        for (auto& node : m_nodes)
        {
            node.second.nodeIndex = linearizedNodes.size();
            linearizedNodes.emplace_back(&node);
        }

        graph_error error{};

        // Recursive function for topological sort based on DFS visit
        const auto dfs = [this, &error, &linearizedNodes, &nodeStates, &nodesOrder](auto&& recurse, usize nodeIndex)
        {
            auto& nodeState = nodeStates[nodeIndex];

            if (nodeState == visit_state::visited)
            {
                return true;
            }

            if (nodeState == visit_state::visiting)
            {
                error = graph_error::not_a_dag;
                return false;
            }

            auto& [type, nodeDesc] = *linearizedNodes[nodeIndex];

            nodeState = visit_state::visiting;

            for (const auto& outEdge : nodeDesc.outEdges)
            {
                const auto it = m_nodes.find(outEdge.targetNode);

                if (it == m_nodes.end())
                {
                    return false;
                }

                if (!recurse(recurse, it->second.nodeIndex))
                {
                    return false;
                }
            }

            nodesOrder.emplace_back(nodeIndex);
            nodeState = visit_state::visited;

            return true;
        };

        for (usize i = 0u; i < numNodes; ++i)
        {
            if (!dfs(dfs, i))
            {
                return error;
            }
        }

        std::reverse(nodesOrder.begin(), nodesOrder.end());

        render_graph g;

        // We should check it maybe instead
        OBLO_ASSERT(m_allocationSize != 0);

        // TODO: Cleanup function to call destructors on failure
        g.m_allocator = std::make_unique<std::byte[]>(m_allocationSize);

        auto allocateGraphData = [current = static_cast<void*>(g.m_allocator.get()),
                                     space = m_allocationSize](usize size, usize alignment) mutable
        {
            auto* result = std::align(alignment, size, current, space);
            current = static_cast<u8*>(current) + size;
            OBLO_ASSERT(result);
            return result;
        };

        const auto createGraphData = [&allocateGraphData](const type_desc& typeDesc)
        {
            auto* const ptr = allocateGraphData(typeDesc.size, typeDesc.alignment);
            typeDesc.construct(ptr);
            return ptr;
        };

        for (usize i = 0u; i < numNodes; ++i)
        {
            auto& [type, nodeDesc] = *linearizedNodes[i];

            nodeDesc.node = createGraphData(nodeDesc.typeDesc);

            for (const auto& pin : nodeDesc.pins)
            {
                write_u32(nodeDesc.node, pin.offset, pin.id);
            }
        }

        g.m_nodes.reserve(m_nodes.size());

        const u32 numPins = m_nextDataId;

        // Propagate pin connections
        // Graph inputs will all have their own entry in g.m_textureResources
        // Connected pins will point to the same texture in the g.m_textureResources array
        // Unconnected pins will have their own entry, and will be expected to be created at runtime
        g.m_inputs.reserve(m_inputs.size() + 1);

        // The index 0 is used as invalid index, so we leave it empty for convenience
        g.m_pinStorage.emplace_back();

        g.m_pins.resize(numPins);

        for (const auto& input : m_inputs)
        {
            auto& dataInput = g.m_inputs.emplace_back();
            const u32 storageIndex = u32(g.m_pinStorage.size());
            dataInput.storageIndex = storageIndex;
            dataInput.name = input.name;

            auto* const ptr = allocateGraphData(input.typeDesc.size, input.typeDesc.alignment);
            input.typeDesc.construct(ptr);

            g.m_pinStorage.emplace_back(ptr, input.typeDesc.destruct);

            for (const auto& edge : input.outEdges)
            {
                const auto it = m_nodes.find(edge.targetNode);

                if (it == m_nodes.end())
                {
                    return graph_error::node_not_found;
                }

                const auto idTarget = read_u32(it->second.node, edge.targetOffset);
                g.m_pins[idTarget].storageIndex = storageIndex;
            }
        }

        std::vector<bool> visitedPins(numPins, false);

        for (const usize nodeIndex : nodesOrder)
        {
            // Look at the nodes in here, if they are not connected, they should be backed by a texture resource
            // that needs to be created at runtime
            auto& nodeDesc = linearizedNodes[nodeIndex]->second;

            for (u32 dataPin = nodeDesc.firstPin; dataPin != nodeDesc.lastPin; ++dataPin)
            {
                if (auto& pin = g.m_pins[dataPin]; pin.storageIndex == 0)
                {
                    pin.storageIndex = u32(g.m_pinStorage.size());
                    const auto pinIndex = dataPin - nodeDesc.firstPin;
                    const auto& typeDesc = nodeDesc.pins[pinIndex].typeDesc;
                    g.m_pinStorage.emplace_back(createGraphData(typeDesc), typeDesc.destruct);
                }
            }

            auto* const nodeSourcePtr = nodeDesc.node;

            // Propagate the resources to the connected pins
            for (auto& edge : nodeDesc.outEdges)
            {
                const auto it = m_nodes.find(edge.targetNode);

                if (it == m_nodes.end())
                {
                    return graph_error::node_not_found;
                }

                const auto idSource = read_u32(nodeSourcePtr, edge.sourceOffset);
                const auto idTarget = read_u32(it->second.node, edge.targetOffset);

                // The target will point to the same resource as the source, which might be coming
                // from an input or created by another node
                g.m_pins[idTarget] = g.m_pins[idSource];
            }
        }

        g.m_outputs.reserve(m_outputs.size());

        for (const auto& output : m_outputs)
        {
            if (output.inEdges.empty())
            {
                continue;
            }

            auto& incoming = output.inEdges.front();
            const auto it = m_nodes.find(incoming.targetNode);

            if (it == m_nodes.end())
            {
                return graph_error::node_not_found;
            }

            const auto idTarget = read_u32(it->second.node, incoming.targetOffset);

            auto& dataOutput = g.m_outputs.emplace_back();
            dataOutput.storageIndex = g.m_pins[idTarget].storageIndex;
            dataOutput.name = output.name;
        }

        for (const usize nodeIndex : nodesOrder)
        {
            auto& [type, nodeDesc] = *linearizedNodes[nodeIndex];

            auto& node = g.m_nodes.emplace_back();
            node.node = std::move(nodeDesc.node);
            node.typeId = type;
            node.init = nodeDesc.init;
            node.build = nodeDesc.build;
            node.execute = nodeDesc.execute;
            node.destruct = nodeDesc.typeDesc.destruct;
        }

        return std::move(g);
    }
}