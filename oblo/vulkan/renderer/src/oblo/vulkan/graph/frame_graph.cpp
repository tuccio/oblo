#include <oblo/vulkan/graph/frame_graph.hpp>

#include <oblo/core/buffered_array.hpp>
#include <oblo/core/frame_allocator.hpp>
#include <oblo/core/graph/dfs_visit.hpp>
#include <oblo/core/graph/dot.hpp>
#include <oblo/core/iterator/reverse_range.hpp>
#include <oblo/core/iterator/zip_range.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/trace/profile.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/graph/frame_graph_context.hpp>
#include <oblo/vulkan/graph/frame_graph_impl.hpp>
#include <oblo/vulkan/graph/frame_graph_template.hpp>
#include <oblo/vulkan/renderer.hpp>
#include <oblo/vulkan/stateful_command_buffer.hpp>

#include <sstream>

namespace oblo::vk
{
    namespace
    {
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

        if (!m_impl->graph.has_edge(srcNode, dstNode))
        {
            m_impl->graph.add_edge(srcNode, dstNode);
        }

        return true;
    }

    h32<frame_graph_subgraph> frame_graph::instantiate(const frame_graph_template& graphTemplate)
    {
        const auto [it, key] = m_impl->subgraphs.emplace();

        auto& frameGraph = m_impl->graph;
        const auto& templateGraph = graphTemplate.get_graph();

        const h32<frame_graph_subgraph> subgraph{key};

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

                *nodeIt = {
                    .node = nodePtr,
                    .build = nodeDesc.build,
                    .execute = nodeDesc.execute,
                    .init = nodeDesc.init,
                    .destruct = nodeDesc.typeDesc.destruct,
                    .typeId = src.nodeDesc.typeDesc.typeId,
                    .size = nodeDesc.typeDesc.size,
                    .alignment = nodeDesc.typeDesc.alignment,
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
            // We don't allocate storage yet, this will happen when building, only if necessary

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
            const auto inVertex = *it->templateToInstanceMap.try_find(in);
            it->inputs.emplace(templateGraph[in].name, inVertex);
        }

        for (const auto out : graphTemplate.get_outputs())
        {
            const auto outVertex = *it->templateToInstanceMap.try_find(out);
            it->outputs.emplace(templateGraph[out].name, outVertex);
        }

        return subgraph;
    }

    void frame_graph::remove(h32<frame_graph_subgraph> graph)
    {
        auto* const g = m_impl->subgraphs.try_find(graph);

        if (!g)
        {
            return;
        }

        // TODO: (#8) This could be a set
        using edge_handle = frame_graph_topology::edge_handle;

        buffered_array<edge_handle, 32> edgesToRemove;

        for (const auto v : g->templateToInstanceMap.values())
        {
            const auto& vertexData = m_impl->graph[v];

            switch (vertexData.kind)
            {
            case frame_graph_vertex_kind::node:
                if (vertexData.node)
                {
                    auto& node = m_impl->nodes.at(vertexData.node);

                    if (node.node)
                    {
                        if (node.destruct)
                        {
                            node.destruct(node.node);
                        }

                        m_impl->memoryPool.deallocate(node.node, node.size, node.alignment);
                        m_impl->nodes.erase(vertexData.node);
                    }
                }

                break;

            case frame_graph_vertex_kind::pin:
                if (vertexData.pin)
                {
                    auto& pin = m_impl->pins.at(vertexData.pin);
                    auto& storage = m_impl->pinStorage.at(pin.ownedStorage);

                    m_impl->free_pin_storage(storage, false);

                    m_impl->pinStorage.erase(pin.ownedStorage);
                    m_impl->pins.erase(vertexData.pin);
                }
                break;
            }

            edgesToRemove.clear();

            for (const auto e : m_impl->graph.get_out_edges(v))
            {
                if (auto* const src = m_impl->graph.try_get(e.vertex))
                {
                    edgesToRemove.push_back(e.handle);
                }
            }

            for (const auto e : m_impl->graph.get_in_edges(v))
            {
                if (auto* const dst = m_impl->graph.try_get(e.vertex))
                {
                    edgesToRemove.push_back(e.handle);
                }
            }

            for (const auto e : edgesToRemove)
            {
                m_impl->graph.remove_edge(e);
            }

            m_impl->graph.remove_vertex(v);
        }
    }

    bool frame_graph::init(vulkan_context& ctx)
    {
        // Just arbitrary fixed size for now
        constexpr u32 maxAllocationSize{64u << 20};

        m_impl = std::make_unique<frame_graph_impl>();

        return m_impl->dynamicAllocator.init(maxAllocationSize) && m_impl->resourcePool.init(ctx);
    }

    void frame_graph::shutdown(vulkan_context& ctx)
    {
        for (const auto& node : m_impl->nodes.values())
        {
            if (!node.node)
            {
                continue;
            }

            if (node.destruct)
            {
                node.destruct(node.node);
            }

            // We are deallocating just for keeping track, it's not really necessary
            m_impl->memoryPool.deallocate(node.node, node.size, node.alignment);
        }

        for (const auto& pinStorage : m_impl->pinStorage.values())
        {
            m_impl->free_pin_storage(pinStorage, false);
        }

        m_impl->resourcePool.shutdown(ctx);
        m_impl->dynamicAllocator.shutdown();

        m_impl.reset();
    }

    void frame_graph::build(renderer& renderer)
    {
        OBLO_PROFILE_SCOPE("frame_graph::execute");

        m_impl->dynamicAllocator.restore_all();

        m_impl->rebuild_runtime(renderer);

        m_impl->nodeTransitions.assign(m_impl->sortedNodes.size(), {});

        const frame_graph_build_context buildCtx{*m_impl, renderer, m_impl->resourcePool};

        // This is not really necessary, we might just do it in debug
        for (auto& ps : m_impl->pinStorage.values())
        {
            ps.poolIndex = ~u32{};
        }

        // The two calls are from a time where we managed multiple small graphs sharing the resource pool, rather than 1
        // big graph owning it.
        m_impl->resourcePool.begin_build();
        m_impl->resourcePool.begin_graph();

        u32 nodeIndex{};

        for (const auto& node : m_impl->sortedNodes)
        {
            auto* const ptr = node.node;

            if (node.build)
            {
                OBLO_PROFILE_SCOPE("Build");
                OBLO_PROFILE_TAG(node.typeId.name);

                auto& nodeTransitions = m_impl->nodeTransitions[nodeIndex];
                nodeTransitions.firstTextureTransition = u32(m_impl->textureTransitions.size());
                nodeTransitions.firstBufferBarrier = u32(m_impl->bufferBarriers.size());

                node.build(ptr, buildCtx);

                nodeTransitions.lastTextureTransition = u32(m_impl->textureTransitions.size());
                nodeTransitions.lastBufferBarrier = u32(m_impl->bufferBarriers.size());
            }

            ++nodeIndex;
        }

        m_impl->resourcePool.end_graph();
        m_impl->resourcePool.end_build(renderer.get_vulkan_context());
    }

    void frame_graph::execute(renderer& renderer)
    {
        OBLO_PROFILE_SCOPE("Execute");

        auto& commandBuffer = renderer.get_active_command_buffer();
        auto& resourcePool = m_impl->resourcePool;

        const frame_graph_execute_context executeCtx{*m_impl, renderer, commandBuffer.get()};

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

        command_buffer_state commandBufferState;

        for (const auto [resource, poolIndex] : m_impl->transientTextures)
        {
            constexpr VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            const auto tex = resourcePool.get_texture(poolIndex);

            commandBufferState.set_starting_layout(h32<texture>{resource.value}, initialLayout);

            new (m_impl->access_storage(resource)) texture{tex};
        }

        buffered_array<VkBufferMemoryBarrier2, 32> bufferBarriers;

        flat_dense_map<h32<frame_graph_pin_storage>,
            frame_graph_buffer_barrier,
            decltype(m_impl->pinStorage)::extractor_type>
            bufferStates;

        for (auto&& [node, transitions] : zip_range(m_impl->sortedNodes, m_impl->nodeTransitions))
        {
            auto* const ptr = node.node;

            for (u32 i = transitions.firstTextureTransition; i != transitions.lastTextureTransition; ++i)
            {
                const auto& textureTransition = m_impl->textureTransitions[i];

                const auto* const texturePtr = static_cast<texture*>(m_impl->access_storage(textureTransition.texture));
                OBLO_ASSERT(texturePtr && texturePtr->image);

                commandBufferState.add_pipeline_barrier(*texturePtr,
                    h32<texture>{textureTransition.texture.value},
                    commandBuffer.get(),
                    textureTransition.target);
            }

            bufferBarriers.clear();

            for (u32 i = transitions.firstBufferBarrier; i < transitions.lastBufferBarrier; ++i)
            {
                const auto& dst = m_impl->bufferBarriers[i];

                const auto [it, inserted] = bufferStates.emplace(dst.buffer);

                if (!inserted)
                {
                    // The buffer was already tracked, add the pipeline barrier
                    auto& src = *it;

                    const auto* const bufferPtr = static_cast<buffer*>(m_impl->access_storage(dst.buffer));
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
                    bufferStates.emplace(dst.buffer, dst);
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
                OBLO_PROFILE_SCOPE("Execute");
                OBLO_PROFILE_TAG(node.typeId.name);
                node.execute(ptr, executeCtx);
            }
        }

        m_impl->finish_frame();
    }

    void frame_graph::write_dot(std::ostream& os) const
    {
        m_impl->write_dot(os);
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

    void frame_graph_impl::add_transient_resource(resource<texture> handle, u32 poolIndex)
    {
        const auto storage = to_storage_handle(handle);
        transientTextures.emplace_back(storage, poolIndex);
        pinStorage.at(storage).poolIndex = poolIndex;
    }

    void frame_graph_impl::add_resource_transition(resource<texture> handle, VkImageLayout target)
    {
        const auto storage = to_storage_handle(handle);
        textureTransitions.emplace_back(storage, target);
    }

    u32 frame_graph_impl::find_pool_index(resource<texture> handle) const
    {
        const auto storage = to_storage_handle(handle);
        return pinStorage.at(storage).poolIndex;
    }

    u32 frame_graph_impl::find_pool_index(resource<buffer> handle) const
    {
        const auto storage = to_storage_handle(handle);
        return pinStorage.at(storage).poolIndex;
    }

    void frame_graph_impl::add_transient_buffer(
        resource<buffer> handle, u32 poolIndex, const staging_buffer_span* upload)
    {
        const auto storage = to_storage_handle(handle);
        transientBuffers.emplace_back(storage, poolIndex);
        pinStorage.at(storage).poolIndex = poolIndex;

        if (upload)
        {
            pendingUploads.emplace_back(storage, *upload);
        }
    }

    void frame_graph_impl::add_buffer_access(
        resource<buffer> handle, VkPipelineStageFlags2 pipelineStage, VkAccessFlags2 access)
    {
        const auto storage = to_storage_handle(handle);

        OBLO_ASSERT(storage);
        OBLO_ASSERT(pinStorage.try_find(storage));

        bufferBarriers.push_back({
            .buffer = storage,
            .pipelineStage = pipelineStage,
            .access = access,
        });
    }

    h32<frame_graph_pin_storage> frame_graph_impl::allocate_dynamic_resource_pin()
    {
        const auto [storage, key] = pinStorage.emplace();
        const auto handle = h32<frame_graph_pin_storage>{key};

        dynamicPins.emplace_back(handle);

        return handle;
    }

    void* frame_graph_impl::access_storage(h32<frame_graph_pin_storage> handle) const
    {
        const auto& storage = pinStorage.at(handle);
        return storage.data;
    }

    void frame_graph_impl::rebuild_runtime(renderer& renderer)
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

            OBLO_ASSERT(pinVertex.pin);

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

    void frame_graph_impl::flush_uploads(VkCommandBuffer commandBuffer, staging_buffer& stagingBuffer)
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

    void frame_graph_impl::finish_frame()
    {
        for (auto& h : dynamicPins)
        {
            const auto& storage = pinStorage.at(h);
            free_pin_storage(storage, true);
            pinStorage.erase(h);
        }

        bufferBarriers.clear();
        textureTransitions.clear();
        transientTextures.clear();
        transientBuffers.clear();
        dynamicPins.clear();
    }

    void frame_graph_impl::free_pin_storage(const frame_graph_pin_storage& storage, bool isFrameAllocated)
    {
        if (storage.data && storage.typeDesc.destruct)
        {
            storage.typeDesc.destruct(storage.data);

            // These pointers should always come from the frame allocator, no need to delete them
            OBLO_ASSERT(isFrameAllocated == dynamicAllocator.contains(storage.data));

            if (!isFrameAllocated)
            {
                memoryPool.deallocate(storage.data, storage.typeDesc.size, storage.typeDesc.alignment);
            }
        }
    }

    void frame_graph_impl::write_dot(std::ostream& os) const
    {
        write_graphviz_dot(os,
            graph,
            [this](const frame_graph_topology::vertex_handle v) -> std::string_view
            {
                const auto& vertex = graph[v];

                switch (vertex.kind)
                {
                case frame_graph_vertex_kind::node:
                    return nodes.try_find(vertex.node)->typeId.name;

                case frame_graph_vertex_kind::pin: {
                    OBLO_ASSERT(vertex.pin);
                    const auto storage = pins.try_find(vertex.pin)->ownedStorage;
                    return pinStorage.try_find(storage)->typeDesc.typeId.name;
                }

                default:
                    unreachable();
                }
            });
    }
}