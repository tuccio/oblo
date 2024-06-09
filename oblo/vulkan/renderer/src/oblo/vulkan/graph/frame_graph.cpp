#include <oblo/vulkan/graph/frame_graph.hpp>

#include <oblo/core/buffered_array.hpp>
#include <oblo/core/frame_allocator.hpp>
#include <oblo/core/graph/directed_graph.hpp>
#include <oblo/core/graph/dot.hpp>
#include <oblo/core/graph/topological_sort.hpp>
#include <oblo/core/iterator/reverse_range.hpp>
#include <oblo/core/iterator/zip_range.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/graph/frame_graph_context.hpp>
#include <oblo/vulkan/graph/frame_graph_data.hpp>
#include <oblo/vulkan/graph/frame_graph_template.hpp>
#include <oblo/vulkan/renderer.hpp>
#include <oblo/vulkan/resource_manager.hpp>
#include <oblo/vulkan/stateful_command_buffer.hpp>

#include <memory_resource>
#include <sstream>
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
            type_id typeId;
            bool initialized;
        };

        struct frame_graph_vertex
        {
            frame_graph_vertex_kind kind;
            h32<frame_graph_node> node;
            h32<frame_graph_pin> pin;
        };
        struct frame_graph_texture_transition
        {
            h32<frame_graph_pin_storage> texture;
            VkImageLayout target;
        };

        struct frame_graph_buffer_barrier
        {
            h32<frame_graph_pin_storage> buffer;
            VkPipelineStageFlags2 pipelineStage;
            VkAccessFlags2 access;
        };

        struct frame_graph_texture
        {
            h32<frame_graph_pin_storage> texture;
            u32 poolIndex;
        };

        struct frame_graph_buffer
        {
            h32<frame_graph_pin_storage> buffer;
            u32 poolIndex;
        };

        struct frame_graph_node_transitions
        {
            u32 firstTextureTransition;
            u32 lastTextureTransition;
            u32 firstBufferBarrier;
            u32 lastBufferBarrier;
        };

        struct frame_graph_pending_upload
        {
            h32<frame_graph_pin_storage> buffer;
            staging_buffer_span source;
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

        void write_u32(void* ptr, u32 offset, u32 value)
        {
            std::memcpy(static_cast<u8*>(ptr) + offset, &value, sizeof(u32));
        }

        template <typename T>
        h32<frame_graph_pin_storage> to_storage_handle(h32<resource_pin<T>> h)
        {
            return h32<frame_graph_pin_storage>{h.value};
        }

    }

    using name_to_vertex_map =
        std::unordered_map<std::string, frame_graph_topology::vertex_handle, transparent_string_hash, std::equal_to<>>;

    struct frame_graph_subgraph
    {
        flat_dense_map<frame_graph_template_vertex_handle, frame_graph_topology::vertex_handle> templateToInstanceMap;
        name_to_vertex_map inputs;
        name_to_vertex_map outputs;
    };

    struct frame_graph_pin
    {
        h32<frame_graph_pin_storage> ownedStorage;
        h32<frame_graph_pin_storage> referencedPin;
        frame_graph_topology::vertex_handle nodeHandle;
        u32 pinMemberOffset;
    };

    struct frame_graph_pin_storage
    {
        void* data;
        // Only valid during a frame for transient resources
        u32 poolIndex;
        frame_graph_data_desc typeDesc;
    };

    struct frame_graph::impl
    {
        // Topology
    public:
        directed_graph<frame_graph_vertex> graph;
        h32_flat_pool_dense_map<frame_graph_node> nodes;
        h32_flat_pool_dense_map<frame_graph_pin> pins;
        h32_flat_pool_dense_map<frame_graph_pin_storage> pinStorage;
        h32_flat_pool_dense_map<frame_graph_subgraph> subgraphs;

        std::pmr::unsynchronized_pool_resource memoryPool;

        // Runtime
    public:
        frame_allocator dynamicAllocator;
        resource_manager* resourceManager{};

        dynamic_array<frame_graph_node> sortedNodes;
        h32_flat_pool_dense_map<frame_graph_texture> textures;
        h32_flat_pool_dense_map<frame_graph_buffer> buffers;

        dynamic_array<frame_graph_node_transitions> nodeTransitions;
        dynamic_array<frame_graph_buffer_barrier> bufferBarriers;
        dynamic_array<frame_graph_texture_transition> textureTransitions;
        dynamic_array<frame_graph_texture> transientTextures;
        dynamic_array<frame_graph_buffer> transientBuffers;
        dynamic_array<frame_graph_pending_upload> pendingUploads;
        dynamic_array<h32<frame_graph_pin_storage>> dynamicPins;

    public:
        void rebuild_runtime(renderer& renderer);
        void flush_uploads(VkCommandBuffer commandBuffer, staging_buffer& stagingBuffer);
        void finish_frame();

        [[maybe_unused]] std::string to_graphviz_dot() const;
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
                    .typeId = src.nodeDesc.typeDesc.typeId,
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
                .pinMemberOffset = src.pinMemberOffset,
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

    void frame_graph::build(renderer& renderer, resource_pool& resourcePool)
    {
        m_impl->dynamicAllocator.restore_all();

        m_impl->rebuild_runtime(renderer);

        m_impl->nodeTransitions.assign(m_impl->sortedNodes.size(), {});

        const frame_graph_build_context buildCtx{*this, renderer, resourcePool};

        // This is not really necessary, we might just do it in debug
        for (auto& ps : m_impl->pinStorage.values())
        {
            ps.poolIndex = ~u32{};
        }

        u32 nodeIndex{};

        for (const auto& node : m_impl->sortedNodes)
        {
            auto* const ptr = node.node;

            if (node.build)
            {
                auto& nodeTransitions = m_impl->nodeTransitions[nodeIndex];
                nodeTransitions.firstTextureTransition = u32(m_impl->textureTransitions.size());
                nodeTransitions.firstBufferBarrier = u32(m_impl->bufferBarriers.size());

                node.build(ptr, buildCtx);

                nodeTransitions.lastTextureTransition = u32(m_impl->textureTransitions.size());
                nodeTransitions.lastBufferBarrier = u32(m_impl->bufferBarriers.size());
            }

            ++nodeIndex;
        }

        // for (const auto& pendingCopy : m_impl->pendingCopies)
        //{
        //     const u32 poolIndex = m_impl->resourcePoolId[pendingCopy.sourceStorageIndex];
        //     resourcePool.add_usage(poolIndex, VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
        // }
    }

    void frame_graph::execute(renderer& renderer, resource_pool& resourcePool)
    {
        auto& resourceManager = renderer.get_resource_manager();
        auto& commandBuffer = renderer.get_active_command_buffer();

        const frame_graph_execute_context executeCtx{*this, renderer, commandBuffer.get()};

        for (const auto [storage, poolIndex] : m_impl->transientBuffers)
        {
            const buffer buf = resourcePool.get_buffer(poolIndex);

            auto& data = m_impl->pinStorage.at(storage);

            // This should only really happen for dynamic pins, we could consider adding some debug check though
            if (!data.data)
            {
                data.data = m_impl->dynamicAllocator.allocate(sizeof(buffer), alignof(buffer));
            }

            new (data.data) buffer{buf};
        }

        if (!m_impl->pendingUploads.empty())
        {
            m_impl->flush_uploads(commandBuffer.get(), renderer.get_staging_buffer());
        }

        for (const auto [resource, poolIndex] : m_impl->transientTextures)
        {
            constexpr VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            const auto tex = resourcePool.get_texture(poolIndex);
            // TODO: Unregister them
            const auto handle = resourceManager.register_texture(tex, initialLayout);
            commandBuffer.set_starting_layout(handle, initialLayout);

            new (access_storage(resource)) h32<texture>{handle};
        }

        buffered_array<VkBufferMemoryBarrier2, 32> bufferBarriers;

        flat_dense_map<h32<frame_graph_pin_storage>, frame_graph_buffer_barrier> m_bufferStates;

        for (auto&& [node, transitions] : zip_range(m_impl->sortedNodes, m_impl->nodeTransitions))
        {
            auto* const ptr = node.node;

            for (u32 i = transitions.firstTextureTransition; i != transitions.lastTextureTransition; ++i)
            {
                const auto& textureTransition = m_impl->textureTransitions[i];

                const auto* const texturePtr = static_cast<h32<texture>*>(access_storage(textureTransition.texture));
                OBLO_ASSERT(texturePtr && *texturePtr);

                commandBuffer.add_pipeline_barrier(resourceManager, *texturePtr, textureTransition.target);
            }

            bufferBarriers.clear();

            for (u32 i = transitions.firstBufferBarrier; i < transitions.lastBufferBarrier; ++i)
            {
                const auto& dst = m_impl->bufferBarriers[i];

                const auto [it, inserted] = m_bufferStates.emplace(dst.buffer);

                if (!inserted)
                {
                    // The buffer was already tracked, add the pipeline barrier
                    auto& src = *it;

                    const auto* const bufferPtr = static_cast<buffer*>(access_storage(dst.buffer));
                    OBLO_ASSERT(bufferPtr && bufferPtr->buffer);

                    bufferBarriers.push_back({
                        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                        .srcStageMask = src.pipelineStage,
                        .srcAccessMask = src.access,
                        .dstStageMask = dst.pipelineStage,
                        .dstAccessMask = dst.access,
                        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .buffer = bufferPtr->buffer,
                        .offset = bufferPtr->offset,
                        .size = bufferPtr->size,
                    });

                    // Track the new state as the new buffer state
                    *it = dst;
                }
                else
                {
                    // First time we encounter the buffer, add it to the states
                    m_bufferStates.emplace(dst.buffer, dst);
                }
            }

            if (!bufferBarriers.empty())
            {
                const VkDependencyInfo dependencyInfo{
                    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                    .bufferMemoryBarrierCount = u32(bufferBarriers.size()),
                    .pBufferMemoryBarriers = bufferBarriers.data(),
                };

                vkCmdPipelineBarrier2(commandBuffer.get(), &dependencyInfo);
            }

            if (node.execute)
            {
                node.execute(ptr, executeCtx);
            }
        }

        // Hopefully get rid of this

        // if (!m_pendingCopies.empty())
        //{
        //     auto& vkCtx = renderer.get_vulkan_context();
        //     vkCtx.begin_debug_label(commandBuffer.get(), "render_graph::flush_copies");
        //     flush_image_copies(commandBuffer, resourceManager);
        //     vkCtx.end_debug_label(commandBuffer.get());
        // }

        m_impl->finish_frame();
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

    void frame_graph::add_transient_resource(resource<texture> handle, u32 poolIndex)
    {
        const auto storage = to_storage_handle(handle);
        m_impl->transientTextures.emplace_back(storage, poolIndex);
        m_impl->pinStorage.at(storage).poolIndex = poolIndex;
        // const u32 poolIndex = m_impl->pins.at(to_storage_handle(handle)).poolIndex;
        // m_impl->resourcePoolId[storageIndex] = poolIndex;
    }

    void frame_graph::add_resource_transition(resource<texture> handle, VkImageLayout target)
    {
        const auto storage = to_storage_handle(handle);
        m_impl->textureTransitions.emplace_back(storage, target);
    }

    u32 frame_graph::find_pool_index(resource<texture> handle) const
    {
        const auto storage = to_storage_handle(handle);
        return m_impl->pinStorage.at(storage).poolIndex;
    }

    u32 frame_graph::find_pool_index(resource<buffer> handle) const
    {
        const auto storage = to_storage_handle(handle);
        return m_impl->pinStorage.at(storage).poolIndex;
    }

    void frame_graph::add_transient_buffer(resource<buffer> handle, u32 poolIndex, const staging_buffer_span* upload)
    {
        const auto storage = to_storage_handle(handle);
        m_impl->transientBuffers.emplace_back(storage, poolIndex);
        m_impl->pinStorage.at(storage).poolIndex = poolIndex;
        // const u32 storageIndex = m_pins[handle.value].storageIndex;
        // m_impl->resourcePoolId[storageIndex] = poolIndex;

        if (upload)
        {
            m_impl->pendingUploads.emplace_back(storage, *upload);
        }
    }

    void frame_graph::add_buffer_access(
        resource<buffer> handle, VkPipelineStageFlags2 pipelineStage, VkAccessFlags2 access)
    {
        m_impl->bufferBarriers.push_back({
            .buffer = to_storage_handle(handle),
            .pipelineStage = pipelineStage,
            .access = access,
        });
    }

    h32<frame_graph_pin_storage> frame_graph::allocate_dynamic_resource_pin()
    {
        /* const auto handle = u32(m_impl->pins.size());
         const auto storageIndex = u32(m_impl->pinStorage.size());*/

        // m_pins.emplace_back(storageIndex);
        // m_resourcePoolId.emplace_back(~u32{});

        const auto [storage, key] = m_impl->pinStorage.emplace();
        const auto handle = h32<frame_graph_pin_storage>{key};

        m_impl->dynamicPins.emplace_back(handle);

        return handle;
    }

    frame_allocator& frame_graph::get_frame_allocator() const
    {
        return m_impl->dynamicAllocator;
    }

    void* frame_graph::access_storage(h32<frame_graph_pin_storage> handle) const
    {
        return m_impl->pinStorage.at(handle).data;
    }

    void frame_graph::impl::rebuild_runtime(renderer& renderer)
    {
        const frame_graph_init_context initCtx{renderer};

        sortedNodes.clear();
        sortedNodes.reserve(nodes.size());

        dynamic_array<frame_graph_topology::vertex_handle> reverseSortedPins;
        reverseSortedPins.reserve(pins.size());

        [[maybe_unused]] const auto isDAG =
            dfs_visit_template<graph_visit_flag::post_visit | graph_visit_flag::fail_if_not_dag>(
                graph,
                [this, &reverseSortedPins, &initCtx](frame_graph_topology::vertex_handle v)
                {
                    auto& vertex = graph[v];

                    switch (vertex.kind)
                    {
                    case frame_graph_vertex_kind::pin: {
                        reverseSortedPins.push_back(v);
                    }

                    break;

                    case frame_graph_vertex_kind::node: {
                        OBLO_ASSERT(vertex.node);

                        auto* node = nodes.try_find(vertex.node);
                        OBLO_ASSERT(node);

                        sortedNodes.push_back(*node);

                        // If the node needs to be initialized, this is a good time
                        if (node->init && !node->initialized)
                        {
                            node->init(node->node, initCtx);
                            node->initialized = true;
                        }
                    }

                    break;
                    }
                },
                // TODO: (#45) Make it possible to use frame_allocator for temporary allocations
                get_global_allocator());

        OBLO_ASSERT(isDAG);

        // Clear all references on pins, before propagating them in DFS order
        for (auto& pin : pins.values())
        {
            pin.referencedPin = {};
        }

        // Propagate pin storage from incoming pins, or allocate the owned storage if necessary
        for (const auto v : reverse_range(reverseSortedPins))
        {
            const auto& pinVertex = graph[v];

            if (!pinVertex.pin)
            {
                // Special pin vertices like input/output from graphs are not backed by actual pins
                continue;
            }

            auto* pin = pins.try_find(pinVertex.pin);
            OBLO_ASSERT(pin);

            for (const auto in : graph.get_in_edges(v))
            {
                const auto& inVertex = graph[in.vertex];

                if (inVertex.kind == frame_graph_vertex_kind::pin && inVertex.pin)
                {
                    auto* const inPin = pins.try_find(inVertex.pin);
                    pin->referencedPin = inPin->referencedPin;
                    break;
                }
            }

            if (!pin->referencedPin)
            {
                OBLO_ASSERT(pin->ownedStorage);

                auto* const storage = pinStorage.try_find(pin->ownedStorage);

                if (!storage->data)
                {
                    storage->data = memoryPool.allocate(storage->typeDesc.size, storage->typeDesc.alignment);
                    storage->typeDesc.construct(storage->data);
                }

                pin->referencedPin = pin->ownedStorage;
            }

            // Assign the handle to the node pin as well
            auto& nodeVertex = graph[pin->nodeHandle];
            OBLO_ASSERT(nodeVertex.node);

            auto* const nodePtr = nodes.try_find(nodeVertex.node)->node;
            write_u32(nodePtr, pin->pinMemberOffset, pin->referencedPin.value);
        }

        std::reverse(sortedNodes.begin(), sortedNodes.end());
    }

    void frame_graph::impl::flush_uploads(VkCommandBuffer commandBuffer, staging_buffer& stagingBuffer)
    {
        OBLO_ASSERT(!pendingUploads.empty());

        for (const auto& upload : pendingUploads)
        {
            const auto& storage = pinStorage.at(upload.buffer);
            const auto* const b = reinterpret_cast<buffer*>(storage.data);
            stagingBuffer.upload(commandBuffer, upload.source, b->buffer, b->offset);
        }

        pendingUploads.clear();
    }

    void frame_graph::impl::finish_frame()
    {
        for (auto& h : dynamicPins)
        {
            const auto& storage = pinStorage.at(h);

            if (storage.data && storage.typeDesc.destruct)
            {
                storage.typeDesc.destruct(storage.data);
            }

            pinStorage.erase(h);
        }

        bufferBarriers.clear();
        textureTransitions.clear();
        transientTextures.clear();
        transientBuffers.clear();
        dynamicPins.clear();
    }

    std::string frame_graph::impl::to_graphviz_dot() const
    {
        std::stringstream ss;

        write_graphviz_dot(ss,
            graph,
            [this](const frame_graph_topology::vertex_handle v) -> std::string_view
            {
                const auto& vertex = graph[v];

                switch (vertex.kind)
                {
                case frame_graph_vertex_kind::node:
                    return nodes.try_find(vertex.node)->typeId.name;

                case frame_graph_vertex_kind::pin: {
                    if (!vertex.pin)
                    {
                        return "<in> or <out>";
                    }

                    const auto storage = pins.try_find(vertex.pin)->ownedStorage;
                    return pinStorage.try_find(storage)->typeDesc.typeId.name;
                }

                default:
                    unreachable();
                }
            });

        return ss.str();
    }
}