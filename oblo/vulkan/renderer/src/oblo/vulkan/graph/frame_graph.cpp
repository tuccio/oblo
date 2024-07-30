#include <oblo/vulkan/graph/frame_graph.hpp>

#include <oblo/core/buffered_array.hpp>
#include <oblo/core/frame_allocator.hpp>
#include <oblo/core/graph/dfs_visit.hpp>
#include <oblo/core/graph/dot.hpp>
#include <oblo/core/iterator/reverse_range.hpp>
#include <oblo/core/iterator/zip_range.hpp>
#include <oblo/core/string/string_builder.hpp>
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
        string_view srcName,
        h32<frame_graph_subgraph> dstGraph,
        string_view dstName)
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

                for (const auto& binding : src.bindings)
                {
                    binding(nodePtr);
                }

                *nodeIt = {
                    .ptr = nodePtr,
                    .build = nodeDesc.build,
                    .execute = nodeDesc.execute,
                    .init = nodeDesc.init,
                    .destruct = nodeDesc.typeDesc.destruct,
                    .typeId = src.nodeDesc.typeDesc.typeId,
                    .size = nodeDesc.typeDesc.size,
                    .alignment = nodeDesc.typeDesc.alignment,
                };

                dst.node = h32<frame_graph_node>{nodeKey};

#ifdef OBLO_DEBUG
                dst.debugTypeId = src.nodeDesc.typeDesc.typeId;
#endif
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

#ifdef OBLO_DEBUG
            dst.debugTypeId = src.pinDesc.typeId;
#endif

            // TODO: Check if it's an input or output pin?
            // We don't allocate storage yet, this will happen when building, only if necessary

            const auto [pinStorageIt, pinStorageKey] = m_impl->pinStorage.emplace();

            *pinIt = {
                .ownedStorage = h32<frame_graph_pin_storage>{pinStorageKey},
                .nodeHandle = *it->templateToInstanceMap.try_find(src.nodeHandle),
                .pinMemberOffset = src.pinMemberOffset,
                .clearDataSink = src.clearDataSink,
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

                    if (node.ptr)
                    {
                        if (node.destruct)
                        {
                            node.destruct(node.ptr);
                        }

                        m_impl->memoryPool.deallocate(node.ptr, node.size, node.alignment);
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

        m_impl->subgraphs.erase(graph);
    }

    void frame_graph::disable_all_outputs(h32<frame_graph_subgraph> graph)
    {
        const auto& sg = m_impl->subgraphs.at(graph);

        for (const auto& [name, out] : sg.outputs)
        {
            auto& v = m_impl->graph[out];
            v.state = frame_graph_vertex_state::disabled;
        }
    }

    void frame_graph::set_output_state(h32<frame_graph_subgraph> graph, string_view name, bool enable)
    {
        const auto& sg = m_impl->subgraphs.at(graph);

        const auto it = sg.outputs.find(name);

        OBLO_ASSERT(it != sg.outputs.end());

        if (it != sg.outputs.end())
        {
            auto& v = m_impl->graph[it->second];
            v.state = enable ? frame_graph_vertex_state::enabled : frame_graph_vertex_state::disabled;
        }
    }

    bool frame_graph::init(vulkan_context& ctx)
    {
        // Just arbitrary fixed size for now
        constexpr u32 maxAllocationSize{64u << 20};

        m_impl = std::make_unique<frame_graph_impl>();
        m_impl->rng.seed(42);

        return m_impl->dynamicAllocator.init(maxAllocationSize) && m_impl->resourcePool.init(ctx);
    }

    void frame_graph::shutdown(vulkan_context& ctx)
    {
        for (const auto& node : m_impl->nodes.values())
        {
            if (!node.ptr)
            {
                continue;
            }

            if (node.destruct)
            {
                node.destruct(node.ptr);
            }

            // We are deallocating just for keeping track, it's not really necessary
            m_impl->memoryPool.deallocate(node.ptr, node.size, node.alignment);
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
        OBLO_PROFILE_SCOPE("Frame Graph Build");

        m_impl->dynamicAllocator.restore_all();

        m_impl->mark_active_nodes();
        m_impl->rebuild_runtime(renderer);

        // Reset the buffer usages
        for (auto& node : m_impl->nodes.values())
        {
            node.bufferUsages.clear();
        }

        m_impl->nodeTransitions.assign(m_impl->sortedNodes.size(), {});

        const frame_graph_build_context buildCtx{*m_impl, renderer, m_impl->resourcePool};

        // Clearing these is required for certain operation that query created textures during the build process (e.g.
        // get_texture_initializer).
        // An alternative would be using a few bits as generation id for the handles.
        for (auto& ps : m_impl->pinStorage.values())
        {
            ps.transientBuffer = {};
            ps.transientTexture = {};
        }

        // The two calls are from a time where we managed multiple small graphs sharing the resource pool, rather than 1
        // big graph owning it.
        m_impl->resourcePool.begin_build();

        u32 nodeIndex{};

        for (const auto& [node, handle] : m_impl->sortedNodes)
        {
            m_impl->currentNode = node;

            auto* const ptr = node->ptr;

            if (node->build)
            {
                OBLO_PROFILE_SCOPE("Build Node");
                OBLO_PROFILE_TAG(node->typeId.name);

                auto& nodeTransitions = m_impl->nodeTransitions[nodeIndex];
                nodeTransitions.firstTextureTransition = u32(m_impl->textureTransitions.size());

                node->build(ptr, buildCtx);

                nodeTransitions.lastTextureTransition = u32(m_impl->textureTransitions.size());
            }

            ++nodeIndex;
        }

        m_impl->currentNode = {};

        m_impl->resourcePool.end_build(renderer.get_vulkan_context());

        auto& textureRegistry = renderer.get_texture_registry();

        for (auto& texture : m_impl->bindlessTextures)
        {
            const auto storage = to_storage_handle(texture.texture);
            const auto transientTexture = m_impl->pinStorage.at(storage).transientTexture;

            const auto& t = m_impl->resourcePool.get_transient_texture(transientTexture);

            textureRegistry.set_texture(texture.resident, t.view, image_layout_tracker::deduce_layout(texture.usage));
        }
    }

    void frame_graph::execute(renderer& renderer)
    {
        OBLO_PROFILE_SCOPE("Frame Graph Execute");

        auto& commandBuffer = renderer.get_active_command_buffer();
        auto& resourcePool = m_impl->resourcePool;

        const frame_graph_execute_context executeCtx{*m_impl, renderer, commandBuffer.get()};

        for (const auto [storage, poolIndex] : m_impl->transientBuffers)
        {
            const buffer buf = resourcePool.get_transient_buffer(poolIndex);

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

        auto& imageLayoutTracker = m_impl->imageLayoutTracker;
        imageLayoutTracker.clear();

        for (const auto [resource, poolIndex] : m_impl->transientTextures)
        {
            const auto t = resourcePool.get_transient_texture(poolIndex);

            // We use the pin storage id as texture id because it's unique per texture
            // The frame graph context also assumes this is the case when reading the layout
            imageLayoutTracker.start_tracking(resource, t);

            new (m_impl->access_storage(resource)) texture{t};
        }

        dynamic_array<VkBufferMemoryBarrier2> bufferBarriers{&m_impl->dynamicAllocator};
        bufferBarriers.reserve(64);

        dynamic_array<VkImageMemoryBarrier2> imageBarriers{&m_impl->dynamicAllocator};
        imageBarriers.reserve(64);

        {
            // Global memory barrier to cover all uploads we just flushed
            const VkMemoryBarrier2 uploadMemoryBarrier{
                .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT,
            };

            const VkDependencyInfo dependencyInfo{
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .memoryBarrierCount = 1,
                .pMemoryBarriers = &uploadMemoryBarrier,
            };

            vkCmdPipelineBarrier2(commandBuffer.get(), &dependencyInfo);
        }

        dynamic_array<h32<frame_graph_node>> incomingNodes{&m_impl->dynamicAllocator};
        incomingNodes.reserve(32);

        for (auto&& [nodeToExecute, transitions] : zip_range(m_impl->sortedNodes, m_impl->nodeTransitions))
        {
            m_impl->currentNode = nodeToExecute.node;

            const auto& node = *nodeToExecute.node;
            auto* const ptr = node.ptr;

            imageBarriers.clear();

            for (u32 i = transitions.firstTextureTransition; i != transitions.lastTextureTransition; ++i)
            {
                const auto& textureTransition = m_impl->textureTransitions[i];

                if (!imageLayoutTracker.add_transition(imageBarriers.push_back_default(),
                        textureTransition.texture,
                        node.passKind,
                        textureTransition.usage))
                {
                    imageBarriers.pop_back();
                }
            }

            bufferBarriers.clear();
            incomingNodes.clear();

            for (const auto edge : m_impl->graph.get_in_edges(nodeToExecute.handle))
            {
                const auto& v = m_impl->graph.get(edge.vertex);

                if (!v.node)
                {
                    continue;
                }

                incomingNodes.push_back(v.node);
            }

            for (const auto [storage, usages] :
                zip_range(nodeToExecute.node->bufferUsages.keys(), nodeToExecute.node->bufferUsages.values()))
            {
                VkPipelineStageFlags2 srcStages{};
                VkAccessFlags2 srcAccess{};

                if (usages.uploadedTo)
                {
                    srcStages |= VK_PIPELINE_STAGE_2_TRANSFER_BIT;
                    srcAccess |= VK_ACCESS_2_TRANSFER_WRITE_BIT;
                }

                for (const auto incomingNode : incomingNodes)
                {
                    const auto& inNodeValue = m_impl->nodes.at(incomingNode);

                    if (auto* const usage = inNodeValue.bufferUsages.try_find(storage))
                    {
                        srcStages |= usage->stages;
                        srcAccess |= usage->access;

                        if (usage->uploadedTo)
                        {
                            srcStages |= VK_PIPELINE_STAGE_2_TRANSFER_BIT;
                            srcAccess |= VK_ACCESS_2_TRANSFER_WRITE_BIT;
                        }
                    }
                }

                const auto shouldForwardAccess = usages.stages == VK_PIPELINE_STAGE_2_NONE;

                if (shouldForwardAccess)
                {
                    const auto [it, ok] = nodeToExecute.node->bufferUsages.emplace(storage);

                    *it = {
                        .stages = srcStages,
                        .access = srcAccess,
                    };
                }
                else if (srcStages != 0 && srcAccess != 0 && usages.access != 0 && usages.stages != 0)
                {
                    const auto* const bufferPtr = static_cast<buffer*>(m_impl->access_storage(storage));
                    OBLO_ASSERT(bufferPtr && bufferPtr->buffer);

                    bufferBarriers.push_back({
                        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                        .srcStageMask = srcStages,
                        .srcAccessMask = srcAccess,
                        .dstStageMask = usages.stages,
                        .dstAccessMask = usages.access,
                        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .buffer = bufferPtr->buffer,
                        .offset = bufferPtr->offset,
                        .size = bufferPtr->size,
                    });
                }
            }

            if (!bufferBarriers.empty() || !imageBarriers.empty())
            {
                const VkDependencyInfo dependencyInfo{
                    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                    .bufferMemoryBarrierCount = u32(bufferBarriers.size()),
                    .pBufferMemoryBarriers = bufferBarriers.data(),
                    .imageMemoryBarrierCount = u32(imageBarriers.size()),
                    .pImageMemoryBarriers = imageBarriers.data(),
                };

                vkCmdPipelineBarrier2(commandBuffer.get(), &dependencyInfo);
            }

            if (node.execute)
            {
                OBLO_PROFILE_SCOPE("Execute Node");
                OBLO_PROFILE_TAG(node.typeId.name);
                node.execute(ptr, executeCtx);
            }
        }

        m_impl->currentNode = {};

        auto& textureRegistry = renderer.get_texture_registry();

        for (const auto& texture : m_impl->bindlessTextures)
        {
            textureRegistry.remove(texture.resident);
        }

        m_impl->bindlessTextures.clear();

        m_impl->finish_frame();
    }

    void frame_graph::write_dot(std::ostream& os) const
    {
        m_impl->write_dot(os);
    }

    void* frame_graph::try_get_input(h32<frame_graph_subgraph> graph, string_view name, const type_id& typeId)
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

    void frame_graph_impl::add_transient_resource(resource<texture> handle,
        h32<transient_texture_resource> transientTexture)
    {
        const auto storage = to_storage_handle(handle);
        transientTextures.emplace_back(storage, transientTexture);
        pinStorage.at(storage).transientTexture = transientTexture;
    }

    void frame_graph_impl::add_resource_transition(resource<texture> handle, texture_usage usage)
    {
        const auto storage = to_storage_handle(handle);
        textureTransitions.emplace_back(storage, usage);
    }

    h32<transient_texture_resource> frame_graph_impl::find_pool_index(resource<texture> handle) const
    {
        const auto storage = to_storage_handle(handle);
        return pinStorage.at(storage).transientTexture;
    }

    h32<transient_buffer_resource> frame_graph_impl::find_pool_index(resource<buffer> handle) const
    {
        const auto storage = to_storage_handle(handle);
        return pinStorage.at(storage).transientBuffer;
    }

    void frame_graph_impl::add_transient_buffer(
        resource<buffer> handle, h32<transient_buffer_resource> transientBuffer, const staging_buffer_span* upload)
    {
        const auto storage = to_storage_handle(handle);
        transientBuffers.emplace_back(storage, transientBuffer);
        pinStorage.at(storage).transientBuffer = transientBuffer;

        if (upload)
        {
            pendingUploads.emplace_back(storage, *upload);
        }
    }

    void frame_graph_impl::set_buffer_access(
        resource<buffer> handle, VkPipelineStageFlags2 pipelineStage, VkAccessFlags2 access, bool uploadedTo)
    {
        const auto storage = to_storage_handle(handle);

        OBLO_ASSERT(storage);
        OBLO_ASSERT(pinStorage.try_find(storage));

        const auto [it, ok] = currentNode->bufferUsages.emplace(storage,
            frame_graph_buffer_usage{
                .stages = pipelineStage,
                .access = access,
                .uploadedTo = uploadedTo,
            });

        OBLO_ASSERT(ok);
    }

    bool frame_graph_impl::can_exec_time_upload(resource<buffer> handle) const
    {
        auto* const usage = currentNode->bufferUsages.try_find(to_storage_handle(handle));
        return usage && usage->access == VK_ACCESS_2_TRANSFER_WRITE_BIT &&
            usage->stages == VK_PIPELINE_STAGE_2_TRANSFER_BIT;
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
        const frame_graph_init_context initCtx{*this, renderer};

        sortedNodes.clear();
        sortedNodes.reserve(nodes.size());

        dynamic_array<frame_graph_topology::vertex_handle> sortedPins{&dynamicAllocator};
        sortedPins.reserve(pins.size());

        [[maybe_unused]] const auto isDAG = dfs_visit_template<graph_visit_flag::post_visit |
            graph_visit_flag::reverse | graph_visit_flag::fail_if_not_dag>(
            graph,
            [this, &sortedPins, &initCtx](frame_graph_topology::vertex_handle v)
            {
                auto& vertex = graph[v];

                switch (vertex.kind)
                {
                case frame_graph_vertex_kind::pin: {
                    sortedPins.push_back(v);
                }

                break;

                case frame_graph_vertex_kind::node: {
                    OBLO_ASSERT(vertex.node);

                    // Cull the disabled nodes (or the unvisited ones, which don't contribute to any output)
                    if (vertex.state != frame_graph_vertex_state::enabled)
                    {
                        return;
                    }

                    auto* node = nodes.try_find(vertex.node);
                    OBLO_ASSERT(node);

                    sortedNodes.push_back({node, v});

                    // If the node needs to be initialized, this is a good time
                    if (node->init && !node->initialized)
                    {
                        currentNode = node;
                        node->init(node->ptr, initCtx);
                        node->initialized = true;
                        currentNode = {};
                    }
                }

                break;
                }
            },
            // TODO: (#45) Make it possible to use frame_allocator for temporary allocations
            &dynamicAllocator);

        OBLO_ASSERT(isDAG);

        // Clear all references on pins, before propagating them in DFS order
        for (auto& pin : pins.values())
        {
            pin.referencedPin = {};
        }

        // Propagate pin storage from incoming pins, or allocate the owned storage if necessary
        const auto propagatePins = [this](auto&& pinsRange, bool processSinks)
        {
            for (const auto v : pinsRange)
            {
                const auto& pinVertex = graph[v];

                OBLO_ASSERT(pinVertex.pin);

                auto* pin = pins.try_find(pinVertex.pin);
                OBLO_ASSERT(pin);

                if ((pin->clearDataSink && !processSinks) || (!pin->clearDataSink && processSinks))
                {
                    continue;
                }

                const auto& edges = processSinks ? graph.get_out_edges(v) : graph.get_in_edges(v);

                for (const auto in : edges)
                {
                    const auto& inVertex = graph[in.vertex];

                    if (inVertex.kind == frame_graph_vertex_kind::pin && inVertex.pin)
                    {
                        auto* const inPin = pins.try_find(inVertex.pin);
                        pin->referencedPin = inPin->referencedPin;
                        break;
                    }
                }

                const bool hasIncomingReference = bool{pin->referencedPin};

                if (!hasIncomingReference)
                {
                    OBLO_ASSERT(pin->ownedStorage);

                    auto* const storage = pinStorage.try_find(pin->ownedStorage);

                    if (!storage->data)
                    {
                        storage->data = memoryPool.allocate(storage->typeDesc.size, storage->typeDesc.alignment);
                        storage->typeDesc.construct(storage->data);
                    }

                    pin->referencedPin = pin->ownedStorage;

                    // For data_sink we clear here
                    if (pin->clearDataSink)
                    {
                        pin->clearDataSink(storage->data);
                    }
                }

                // Assign the handle to the node pin as well
                auto& nodeVertex = graph[pin->nodeHandle];
                OBLO_ASSERT(nodeVertex.node);

                auto* const nodePtr = nodes.try_find(nodeVertex.node)->ptr;
                write_u32(nodePtr, pin->pinMemberOffset, pin->referencedPin.value);
            }
        };

        // We propagate regular pins in node execution order
        propagatePins(sortedPins, false);

        // But sinks are propagated in reverse order
        propagatePins(reverse_range(sortedPins), true);
    }

    void frame_graph_impl::mark_active_nodes()
    {
        // First we mark all nodes as unvisited (pins should stay untouched)
        for (const auto node : graph.get_vertices())
        {
            auto& v = graph[node];

            if (v.node)
            {
                v.state = frame_graph_vertex_state::unvisited;
            }
        }

        dynamic_array<frame_graph_topology::vertex_handle> nodesToEnable{&dynamicAllocator};
        nodesToEnable.reserve(graph.get_vertex_count());

        for (const auto& subgraph : subgraphs.values())
        {
            for (const auto& [name, out] : subgraph.outputs)
            {
                auto& v = graph[out];

                OBLO_ASSERT(v.pin && !v.node);
                OBLO_ASSERT(
                    v.state == frame_graph_vertex_state::enabled || v.state == frame_graph_vertex_state::disabled);

                if (v.state == frame_graph_vertex_state::disabled)
                {
                    continue;
                }

                for (const auto& e : graph.get_in_edges(out))
                {
                    nodesToEnable.push_back(e.vertex);
                }
            }
        }

        while (!nodesToEnable.empty())
        {
            const auto vId = nodesToEnable.back();
            nodesToEnable.pop_back();

            auto& v = graph[vId];

            if (v.kind != frame_graph_vertex_kind::node)
            {
                continue;
            }

            if (v.state != frame_graph_vertex_state::unvisited)
            {
                continue;
            }

            v.state = frame_graph_vertex_state::enabled;

            for (const auto& e : graph.get_in_edges(vId))
            {
                nodesToEnable.push_back(e.vertex);
            }
        }
    }

    void frame_graph_impl::flush_uploads(VkCommandBuffer commandBuffer, staging_buffer& stagingBuffer)
    {
        OBLO_ASSERT(!pendingUploads.empty());

        const VkMemoryBarrier2 before{
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        };

        const VkDependencyInfo beforeDependencyInfo{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .memoryBarrierCount = 1u,
            .pMemoryBarriers = &before,
        };

        vkCmdPipelineBarrier2(commandBuffer, &beforeDependencyInfo);

        for (const auto& upload : pendingUploads)
        {
            const auto& storage = pinStorage.at(upload.buffer);
            const auto* const b = reinterpret_cast<buffer*>(storage.data);
            stagingBuffer.upload(commandBuffer, upload.source, b->buffer, b->offset);
        }

        pendingUploads.clear();

        const VkMemoryBarrier2 after{
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT,
        };

        const VkDependencyInfo afterDependencyInfo{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .memoryBarrierCount = 1u,
            .pMemoryBarriers = &after,
        };

        vkCmdPipelineBarrier2(commandBuffer, &afterDependencyInfo);
    }

    void frame_graph_impl::finish_frame()
    {
        for (auto& h : dynamicPins)
        {
            const auto& storage = pinStorage.at(h);
            free_pin_storage(storage, true);
            pinStorage.erase(h);
        }

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
        string_builder builder;

        write_graphviz_dot(os,
            graph,
            [this, &builder](const frame_graph_topology::vertex_handle v) -> const char*
            {
                const auto& vertex = graph[v];

                switch (vertex.kind)
                {
                case frame_graph_vertex_kind::node: {
                    const auto color = vertex.state == frame_graph_vertex_state::enabled ? "green" : "red";

                    builder.clear().format(R"(label="{}" shape="rect" color="{}" )",
                        nodes.at(vertex.node).typeId.name,
                        color);

                    return builder.c_str();
                }

                case frame_graph_vertex_kind::pin: {
                    OBLO_ASSERT(vertex.pin);
                    const auto storage = pins.try_find(vertex.pin)->ownedStorage;

                    builder.clear().format(R"(label="{}" shape="diamond")",
                        pinStorage.at(storage).typeDesc.typeId.name);

                    return builder.c_str();
                }

                default:
                    unreachable();
                }
            });
    }
}