#include <oblo/vulkan/graph/frame_graph.hpp>

#include <oblo/core/graph/directed_graph.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/core/zip_range.hpp>
#include <oblo/vulkan/graph/frame_graph_data.hpp>
#include <oblo/vulkan/graph/frame_graph_template.hpp>

#include <memory_resource>
#include <unordered_map>

namespace oblo::vk
{
    // Graph inputs have a storage
    // Actually even normal pins might have a storage? E.g. something created by the node itself
    // Connecting 2 pins to the same pin is an error
    // Connecting propagates the input

    namespace
    {
        struct frame_graph_vertex;
        using frame_graph_topology = directed_graph<frame_graph_vertex>;

        struct frame_graph_node
        {
            void* node;
            frame_graph_init_fn init;
            frame_graph_build_fn build;
            frame_graph_execute_fn execute;
            frame_graph_destruct_fn destruct;
        };

        struct frame_graph_pin_storage
        {
            void* data;
            frame_graph_data_desc typeDesc;
        };

        struct frame_graph_pin
        {
            h32<frame_graph_pin_storage> ownedStorage;
            frame_graph_topology::vertex_handle nodeHandle;
        };

        struct frame_graph_vertex
        {
            frame_graph_vertex_kind kind;
            h32<frame_graph_node> node;
            h32<frame_graph_pin> pin;
        };

        struct transparent_string_hash
        {
            using is_transparent = void;

            usize operator()(std::string_view str) const
            {
                return std::hash<std::string_view>{}(str);
            }

            usize operator()(const std::string& str) const
            {
                return std::hash<std::string>{}(str);
            }
        };
    }

    using name_to_vertex_map =
        std::unordered_map<std::string, frame_graph_topology::vertex_handle, transparent_string_hash, std::equal_to<>>;

    struct frame_graph_subgraph
    {
        flat_dense_map<frame_graph_template_vertex_handle, frame_graph_topology::vertex_handle> templateToInstanceMap;
        name_to_vertex_map inputs;
        name_to_vertex_map outputs;
    };

    struct frame_graph::impl
    {
        directed_graph<frame_graph_vertex> graph;
        h32_flat_pool_dense_map<frame_graph_node> nodes;
        h32_flat_pool_dense_map<frame_graph_pin> pins;
        h32_flat_pool_dense_map<frame_graph_pin_storage> pinStorage;
        h32_flat_pool_dense_map<frame_graph_subgraph> subgraphs;

        std::pmr::unsynchronized_pool_resource memoryPool;
    };

    frame_graph::frame_graph() = default;

    frame_graph::frame_graph(frame_graph&&) noexcept = default;

    frame_graph& frame_graph::operator=(frame_graph&&) noexcept = default;

    frame_graph::~frame_graph() = default;

    bool frame_graph::connect(h32<frame_graph_subgraph> srcGraph,
        std::string_view srcName,
        h32<frame_graph_subgraph> dstGraph,
        std::string_view dstName)
    {
        auto* const srcGraphPtr = m_impl->subgraphs.try_find(srcGraph);
        auto* const dstGraphPtr = m_impl->subgraphs.try_find(dstGraph);

        if (!srcGraphPtr || !dstGraphPtr)
        {
            return false;
        }

        const auto outIt = srcGraphPtr->outputs.find(srcName);
        const auto inIt = dstGraphPtr->inputs.find(dstName);

        if (outIt == srcGraphPtr->outputs.end() || inIt == dstGraphPtr->inputs.end())
        {
            return false;
        }

        const auto srcPinVertex = outIt->second;
        const auto dstPinVertex = inIt->second;

        const auto& srcPin = m_impl->graph[srcPinVertex];
        const auto& dstPin = m_impl->graph[dstPinVertex];

        OBLO_ASSERT(srcPin.pin);
        OBLO_ASSERT(dstPin.pin);

        const auto srcNode = m_impl->pins.try_find(srcPin.pin)->nodeHandle;
        const auto dstNode = m_impl->pins.try_find(dstPin.pin)->nodeHandle;

        // Connect the pins, as well as the nodes they belong to, to ensure the correct order of execution
        m_impl->graph.add_edge(outIt->second, inIt->second);
        m_impl->graph.add_edge(srcNode, dstNode);

        return true;
    }

    h32<frame_graph_subgraph> frame_graph::instantiate(const frame_graph_template& graphTemplate)
    {
        const auto [it, key] = m_impl->subgraphs.emplace();

        auto& frameGraph = m_impl->graph;
        const auto& templateGraph = graphTemplate.get_graph();

        for (const auto v : templateGraph.get_vertices())
        {
            const auto& src = templateGraph[v];

            const auto instance = frameGraph.add_vertex();
            auto& dst = frameGraph[instance];

            dst = {
                .kind = src.kind,
            };

            switch (src.kind)
            {
            case frame_graph_vertex_kind::node: {
                const auto [nodeIt, nodeKey] = m_impl->nodes.emplace();

                const auto& nodeDesc = src.nodeDesc;

                void* const nodePtr = m_impl->memoryPool.allocate(nodeDesc.typeDesc.size, nodeDesc.typeDesc.alignment);
                nodeDesc.typeDesc.construct(nodePtr);
                // TODO: Init or delay until later?

                *nodeIt = {
                    .node = nodePtr,
                    .init = nodeDesc.init,
                    .build = nodeDesc.build,
                    .execute = nodeDesc.execute,
                    .destruct = nodeDesc.typeDesc.destruct,
                };

                dst.node = h32<frame_graph_node>{nodeKey};
            }
            break;

            case frame_graph_vertex_kind::pin:
                // initialize pins later because we want to be able to resolve nodes
                break;

            default:
                unreachable();
            }

            it->templateToInstanceMap.emplace(v, instance);
        }

        // Now we can actually initialize pins
        for (const auto [source, instance] :
            zip_range(it->templateToInstanceMap.keys(), it->templateToInstanceMap.values()))
        {
            auto& src = templateGraph[source];
            auto& dst = frameGraph[instance];

            OBLO_ASSERT(src.kind == dst.kind);

            // Pins have the node handle set, except the special input/output pins, we can skip those here
            if (src.kind != frame_graph_vertex_kind::pin || !src.nodeHandle)
            {
                continue;
            }

            const auto [pinIt, pinKey] = m_impl->pins.emplace();
            dst.pin = h32<frame_graph_pin>{pinKey};

            // TODO: Check if it's an input or output pin?
            // TODO: Allocate storage? Maybe we can allocate storage when building instead, only if necessary

            const auto [pinStorageIt, pinStorageKey] = m_impl->pinStorage.emplace();

            *pinIt = {
                .ownedStorage = h32<frame_graph_pin_storage>{pinStorageKey},
                .nodeHandle = *it->templateToInstanceMap.try_find(src.nodeHandle),
            };

            *pinStorageIt = {
                .typeDesc = src.pinDesc,
            };
        }

        for (const auto e : templateGraph.get_edges())
        {
            const auto from = templateGraph.get_source(e);
            const auto to = templateGraph.get_destination(e);

            frameGraph.add_edge(*it->templateToInstanceMap.try_find(from), *it->templateToInstanceMap.try_find(to));
        }

        for (const auto in : graphTemplate.get_inputs())
        {
            const auto inVertex = *it->templateToInstanceMap.try_find(in.vertex);
            it->inputs.emplace(templateGraph[in.vertex].name, inVertex);
        }

        for (const auto out : graphTemplate.get_outputs())
        {
            const auto outVertex = *it->templateToInstanceMap.try_find(out.vertex);
            it->outputs.emplace(templateGraph[out.vertex].name, outVertex);
        }

        return h32<frame_graph_subgraph>{key};
    }

    void frame_graph::init()
    {
        m_impl = std::make_unique<impl>();
    }

    void* frame_graph::try_get_input(h32<frame_graph_subgraph> graph, std::string_view name, const type_id& typeId)
    {
        auto* const graphPtr = m_impl->subgraphs.try_find(graph);

        if (!graphPtr)
        {
            return nullptr;
        }

        const auto it = graphPtr->inputs.find(name);

        if (it == graphPtr->inputs.end())
        {
            return nullptr;
        }

        const auto& v = m_impl->graph[it->second];

        if (!v.pin)
        {
            return nullptr;
        }

        const auto& pinData = *m_impl->pins.try_find(v.pin);

        OBLO_ASSERT(pinData.ownedStorage);
        auto& storage = *m_impl->pinStorage.try_find(pinData.ownedStorage);

        if (storage.typeDesc.typeId != typeId)
        {
            return nullptr;
        }

        if (!storage.data)
        {
            void* const dataPtr = m_impl->memoryPool.allocate(storage.typeDesc.size, storage.typeDesc.alignment);
            storage.typeDesc.construct(dataPtr);
            storage.data = dataPtr;
        }

        return storage.data;
    }
}