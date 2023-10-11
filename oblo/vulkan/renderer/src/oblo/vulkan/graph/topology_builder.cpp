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

        render_graph g;

        g.m_nodes.reserve(m_nodes.size());

        const u32 numDataPins = m_virtualDataId;
        const u32 numTexturePins = m_virtualTextureId;

        // Propagate pin connections
        // Graph inputs will all have their own entry in g.m_textureResources
        // Connected pins will point to the same texture in the g.m_textureResources array
        // Unconnected pins will have their own entry, and will be expected to be created at runtime
        g.m_textureInputs.reserve(m_inputs.size() + 1);
        g.m_dataInputs.reserve(m_inputs.size() + 1);

        // The index 0 is used as invalid index, so we leave it empty for convenience
        g.m_textureResources.emplace_back();
        g.m_dataStorage.emplace_back();

        g.m_texturePins.resize(numTexturePins);
        g.m_dataPins.resize(numDataPins);

        for (const auto& input : m_inputs)
        {
            if (input.typeId == get_type_id<h32<texture>>())
            {
                auto& textureInput = g.m_textureInputs.emplace_back();
                textureInput.storageIndex = u32(g.m_textureResources.size());
                textureInput.name = input.name;
                g.m_textureResources.emplace_back();
            }
            else
            {
                auto& dataInput = g.m_dataInputs.emplace_back();
                const u32 storageIndex = u32(g.m_dataStorage.size());
                dataInput.storageIndex = storageIndex;
                dataInput.name = input.name;
                g.m_dataStorage.emplace_back(input.factory());

                for (const auto& edge : input.outEdges)
                {
                    const auto it = m_nodes.find(edge.targetNode);

                    if (it == m_nodes.end())
                    {
                        return graph_error::node_not_found;
                    }

                    const auto idTarget = read_u32(it->second.node.get(), edge.targetOffset);
                    g.m_dataPins[idTarget].storageIndex = storageIndex;
                }
            }
        }

        std::vector<bool> visitedPins;
        visitedPins.reserve(max(numTexturePins, numDataPins));

        visitedPins.assign(numTexturePins, false);

        for (const usize nodeIndex : nodesOrder)
        {
            // Look at the nodes in here, if they are not connected, they should be backed by a texture resource
            // that needs to be created at runtime
            auto& nodeDesc = linearizedNodes[nodeIndex]->second;

            for (u32 texturePin = nodeDesc.firstTexturePin; texturePin != nodeDesc.lastTexturePin; ++texturePin)
            {
                if (auto& pin = g.m_texturePins[texturePin]; pin.storageIndex == 0)
                {
                    pin.storageIndex = u32(g.m_textureResources.size());
                    g.m_textureResources.emplace_back();
                }
            }

            for (u32 dataPin = nodeDesc.firstDataPin; dataPin != nodeDesc.lastDataPin; ++dataPin)
            {
                if (auto& pin = g.m_dataPins[dataPin]; pin.storageIndex == 0)
                {
                    pin.storageIndex = u32(g.m_dataStorage.size());
                    g.m_dataStorage.emplace_back() = nodeDesc.dataFactories[dataPin - nodeDesc.firstDataPin]();
                }
            }

            auto* const nodeSourcePtr = nodeDesc.node.get();

            // Propagate the resources to the connected pins
            for (auto& edge : nodeDesc.outEdges)
            {
                const auto it = m_nodes.find(edge.targetNode);

                if (it == m_nodes.end())
                {
                    return graph_error::node_not_found;
                }

                const auto idSource = read_u32(nodeSourcePtr, edge.sourceOffset);
                const auto idTarget = read_u32(it->second.node.get(), edge.targetOffset);

                if (edge.kind == pin_kind::texture)
                {
                    // The target will point to the same resource as the source, which might be coming
                    // from an input or created by another node
                    g.m_texturePins[idTarget] = g.m_texturePins[idSource];
                }
                else
                {
                    g.m_dataPins[idTarget] = g.m_dataPins[idSource];
                }
            }
        }

        for (const usize nodeIndex : nodesOrder)
        {
            auto& [type, nodeDesc] = *linearizedNodes[nodeIndex];

            auto& node = g.m_nodes.emplace_back();
            node.node = std::move(nodeDesc.node);
            node.typeId = type;
            node.build = nodeDesc.build;
            node.execute = nodeDesc.execute;
        }

        return std::move(g);
    }
}