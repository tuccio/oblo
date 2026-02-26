#include <oblo/renderer/graph/frame_graph.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/buffered_array.hpp>
#include <oblo/core/frame_allocator.hpp>
#include <oblo/core/graph/dfs_visit.hpp>
#include <oblo/core/graph/dot.hpp>
#include <oblo/core/invoke/function_ref.hpp>
#include <oblo/core/iterator/reverse_range.hpp>
#include <oblo/core/iterator/zip_range.hpp>
#include <oblo/core/span.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/gpu/gpu_instance.hpp>
#include <oblo/gpu/structs.hpp>
#include <oblo/gpu/vulkan/utility/image_utils.hpp>
#include <oblo/metrics/async_metrics.hpp>
#include <oblo/renderer/graph/enums.hpp>
#include <oblo/renderer/graph/frame_graph_context.hpp>
#include <oblo/renderer/graph/frame_graph_impl.hpp>
#include <oblo/renderer/graph/frame_graph_resources_impl.hpp>
#include <oblo/renderer/graph/frame_graph_template.hpp>
#include <oblo/renderer/platform/renderer_platform.hpp>
#include <oblo/renderer/renderer.hpp>
#include <oblo/trace/profile.hpp>

namespace oblo
{
    namespace
    {
        // Pretty limited and fixed size at this time, since we don't need more
        static constexpr u32 g_downloadStagingSize = 1u << 10;

        void write_u32(void* ptr, u32 offset, u32 value)
        {
            std::memcpy(static_cast<u8*>(ptr) + offset, &value, sizeof(u32));
        }

        template <typename T>
        h32<frame_graph_pin_storage> to_storage_handle(h32<resource_pin<T>> h)
        {
            return h32<frame_graph_pin_storage>{h.value};
        }

        void build_pass_barriers(frame_graph_impl& impl, dynamic_array<gpu::buffer_memory_barrier>& memoryBarriers)
        {
            memoryBarriers.reserve(128);

            struct buffer_tracking
            {
                flags<gpu::pipeline_sync_stage> previousStages;
                flags<gpu::memory_access_type> previousAccess;
                bool hasMemoryBarrier;
                buffer_access_kind currentAccessKind;
                usize currentBarrierIdx;
            };

            h32_flat_extpool_dense_map<transient_buffer_resource, buffer_tracking> bufferUsages{&impl.dynamicAllocator};
            bufferUsages.reserve_sparse(u32(impl.pinStorage.size() + 1));
            bufferUsages.reserve_dense(u32(impl.pinStorage.size() + 1));

            deque<h32<transient_buffer_resource>> stableBuffers{&impl.dynamicAllocator};

            for (auto& pass : impl.passes)
            {
                const u32 firstBarrierIdx = u32(memoryBarriers.size());

                for (u32 bufferUsageIdx = pass.bufferUsageBegin; bufferUsageIdx < pass.bufferUsageEnd; ++bufferUsageIdx)
                {
                    const auto& bufferUsage = impl.bufferUsages[bufferUsageIdx];

                    // We use the pool index as id because it should identify a buffer
                    // On the contrary, two different pin storages might point to the same buffer in case of rerouting
                    const auto bufferResourceId = impl.pinStorage.at(bufferUsage.pinStorage).transientBuffer;
                    OBLO_ASSERT(bufferResourceId);

                    const auto [tracking, inserted] = bufferUsages.emplace(bufferResourceId);

                    if (inserted && impl.resourcePool.is_stable(bufferResourceId))
                    {
                        // This is a stable buffer, we need to keep track of its usage over multiple frames
                        impl.resourcePool.fetch_buffer_tracking(bufferResourceId,
                            &tracking->previousStages,
                            &tracking->previousAccess,
                            &tracking->currentAccessKind);

                        stableBuffers.push_back(bufferResourceId);
                    }
                    else if (bufferUsage.uploadedTo)
                    {
                        OBLO_ASSERT(!impl.resourcePool.is_stable(bufferResourceId),
                            "We don't support uploading to stable buffers currently, but if we did it would require a "
                            "barrier on the first frame");

                        OBLO_ASSERT(tracking->previousStages.is_empty());

                        tracking->previousStages = gpu::pipeline_sync_stage::transfer;
                        tracking->previousAccess = gpu::memory_access_type::any_write;
                    }

                    // If we don't have an actual usage, we just act like we are "forwarding"
                    if (!bufferUsage.stages.is_empty())
                    {
                        // The usage should be either read or write
                        OBLO_ASSERT(!bufferUsage.access.is_empty());

                        const bool isNewUsageRead = bufferUsage.accessKind == buffer_access_kind::read;
                        const bool isCurrentUsageRead =
                            tracking->hasMemoryBarrier && tracking->currentAccessKind == buffer_access_kind::read;

                        // The current usage might be read, write or none
                        if (isNewUsageRead && isCurrentUsageRead)
                        {
                            // When we already added a read barrier and we find another read usage, we can just add the
                            // stages and access to the previous barrier
                            auto& barrier = memoryBarriers[tracking->currentBarrierIdx];
                            barrier.nextPipelines |= bufferUsage.stages;
                            barrier.nextAccesses |= bufferUsage.access;
                        }
                        else
                        {
                            // When we already had a barrier, we pick up the last one to d determine the previous access
                            // Otherwise we read what tracking is holding (it may be none or transfer due to an upload)
                            if (tracking->hasMemoryBarrier)
                            {
                                auto& barrier = memoryBarriers[tracking->currentBarrierIdx];
                                tracking->previousStages = barrier.nextPipelines;
                                tracking->previousAccess = barrier.nextAccesses;
                            }

                            // Here the current access might be none, or simply different from our new usage (e.g.
                            // read-to-write or write-to-read). We need to add a barrier.
                            const usize newBarrierIdx = memoryBarriers.size();
                            tracking->hasMemoryBarrier = true;
                            tracking->currentBarrierIdx = newBarrierIdx;
                            tracking->currentAccessKind = bufferUsage.accessKind;

                            const auto* const bufferPtr =
                                static_cast<frame_graph_buffer_impl*>(impl.access_storage(bufferUsage.pinStorage));

                            OBLO_ASSERT(bufferPtr && bufferPtr->handle);

                            // When there's no access yet, we can just add a new barrier (we might update access/stage
                            // of it later)
                            memoryBarriers.push_back({
                                .bufferRange = {bufferPtr->handle, bufferPtr->offset, bufferPtr->size},
                                .previousPipelines = tracking->previousStages,
                                .previousAccesses = tracking->previousAccess,
                                .nextPipelines = bufferUsage.stages,
                                .nextAccesses = bufferUsage.access,
                            });
                        }
                    }
                }

                const u32 lastBarrierIdx = u32(memoryBarriers.size());

                pass.bufferBarriersBegin = firstBarrierIdx;
                pass.bufferBarriersEnd = lastBarrierIdx;
            }

            for (const auto stableBufferId : stableBuffers)
            {
                auto* const tracking = bufferUsages.try_find(stableBufferId);
                OBLO_ASSERT(tracking);

                if (tracking && tracking->hasMemoryBarrier)
                {
                    const auto& barrier = memoryBarriers[tracking->currentBarrierIdx];

                    impl.resourcePool.store_buffer_tracking(stableBufferId,
                        barrier.nextPipelines,
                        barrier.nextAccesses,
                        tracking->currentAccessKind);
                }
            }
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

        flat_dense_map<frame_graph_template_vertex_handle, frame_graph_topology::vertex_handle> templateToInstanceMap;

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
                    .subgraph = subgraph,
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

            templateToInstanceMap.emplace(v, instance);
        }

        // Now we can actually initialize pins
        for (const auto [source, instance] : zip_range(templateToInstanceMap.keys(), templateToInstanceMap.values()))
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
                .nodeHandle = *templateToInstanceMap.try_find(src.nodeHandle),
                .pinMemberOffset = src.pinMemberOffset,
                .clearDataSink = src.clearDataSink,
            };

            *pinStorageIt = {
                .typeDesc = src.pinDesc,
                .owner = dst.pin,
            };

            if (!src.bindings.empty())
            {
                void* const dataPtr = m_impl->memoryPool.allocate(src.pinDesc.size, src.pinDesc.alignment);
                pinStorageIt->typeDesc.construct(dataPtr);
                pinStorageIt->data = dataPtr;

                for (auto& binding : src.bindings)
                {
                    binding(dataPtr);
                }
            }
        }

        for (const auto e : templateGraph.get_edges())
        {
            const auto from = templateGraph.get_source(e);
            const auto to = templateGraph.get_destination(e);

            frameGraph.add_edge(*templateToInstanceMap.try_find(from), *templateToInstanceMap.try_find(to));
        }

        for (const auto in : graphTemplate.get_inputs())
        {
            const auto inVertex = *templateToInstanceMap.try_find(in);
            it->inputs.emplace(templateGraph[in].name, inVertex);
        }

        for (const auto out : graphTemplate.get_outputs())
        {
            const auto outVertex = *templateToInstanceMap.try_find(out);
            it->outputs.emplace(templateGraph[out].name, outVertex);
        }

        const std::span allVertices = templateToInstanceMap.values();
        it->vertices.assign(allVertices.begin(), allVertices.end());

        return subgraph;
    }

    void frame_graph::remove(h32<frame_graph_subgraph> graph)
    {
        auto* const g = m_impl->subgraphs.try_find(graph);

        if (!g)
        {
            return;
        }

        // Delete all the dynamic storage
        for (const h32 storage : g->dynamicStorage)
        {
            m_impl->free_pin_storage(storage, m_impl->pinStorage.at(storage), false);
            m_impl->pinStorage.erase(storage);
        }

        using edge_handle = frame_graph_topology::edge_handle;

        buffered_array<edge_handle, 32> edgesToRemove;

        for (const h32 v : g->vertices)
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

                    m_impl->free_pin_storage(pin.ownedStorage, storage, false);

                    m_impl->pinStorage.erase(pin.ownedStorage);
                    m_impl->pins.erase(vertexData.pin);
                }
                break;
            }

            edgesToRemove.clear();

            for (const auto e : m_impl->graph.get_out_edges(v))
            {
                if ([[maybe_unused]] auto* const src = m_impl->graph.try_get(e.vertex))
                {
                    edgesToRemove.push_back(e.handle);
                }
            }

            for (const auto e : m_impl->graph.get_in_edges(v))
            {
                if ([[maybe_unused]] auto* const dst = m_impl->graph.try_get(e.vertex))
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

    bool frame_graph::init(gpu::gpu_instance& gpu)
    {
        // Just arbitrary fixed size for now
        constexpr u32 maxAllocationSize{64u << 20};

        m_impl = allocate_unique<frame_graph_impl>();
        m_impl->rng.seed(42);

        const gpu::device_info deviceInfo = gpu.get_device_info();
        m_impl->gpuInfo.subgroupSize = deviceInfo.subgroupSize;

        return m_impl->dynamicAllocator.init(maxAllocationSize) && m_impl->resourcePool.init(gpu),
               m_impl->downloadStaging.init(gpu, g_downloadStagingSize).has_value();
    }

    void frame_graph::shutdown(gpu::gpu_instance& ctx)
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

        for (const auto& [key, pinStorage] : zip_range(m_impl->pinStorage.keys(), m_impl->pinStorage.values()))
        {
            m_impl->free_pin_storage(key, pinStorage, false);
        }

        m_impl->pinStorage.clear();

        // free_pin_storage might defer destruction of retained textures
        m_impl->free_pending_textures(ctx);

        m_impl->resourcePool.shutdown(ctx);
        m_impl->dynamicAllocator.shutdown();

        m_impl.reset();
    }

    void frame_graph::build(const frame_graph_build_args& args)
    {
        OBLO_PROFILE_SCOPE("Frame Graph Build");

        gpu::gpu_instance& gpu = args.gpu;

        // Free retained textures if the user destroyed some subgraphs that owned some
        m_impl->free_pending_textures(gpu);

        m_impl->dynamicAllocator.restore_all();

        m_impl->mark_active_nodes();
        m_impl->rebuild_runtime(args);

        const u32 extraHiddenNodes = u32{m_impl->is_recording_metrics()};

        m_impl->passesPerNode.clear();
        m_impl->passesPerNode.reserve(m_impl->sortedNodes.size() + extraHiddenNodes);

        // We use pass with index 0 as invalid
        m_impl->passes.assign_default(1);

        frame_graph_build_state buildState{
            .gpu = &gpu,
        };

        const frame_graph_build_context buildCtx{*m_impl, buildState, args};

        // Clearing these is required for certain operations that query created textures during the build process (e.g.
        // get_texture_initializer).
        // An alternative would be using a few bits as generation id for the handles.
        for (auto&& [key, ps] : zip_range(m_impl->pinStorage.keys(), m_impl->pinStorage.values()))
        {
            ps.transientBuffer = {};
            ps.transientTexture = {};
        }

        // The two calls are from a time where we managed multiple small graphs sharing the resource pool, rather than 1
        // big graph owning it.
        m_impl->resourcePool.begin_build();

        // Recreate the transient instances for the retained textures
        for (const h32 retainedTexturePin : m_impl->retainedTextures.keys())
        {
            const h32<frame_graph_pin_storage> storageKey{retainedTexturePin.value};

            auto& ps = m_impl->pinStorage.at(storageKey);
            OBLO_ASSERT(ps.isOwnedTexture);

            const auto* t = static_cast<frame_graph_texture_impl*>(ps.data);
            ps.transientTexture = m_impl->resourcePool.add_external_texture(*t);

            m_impl->transientTextures.emplace_back(storageKey, ps.transientTexture);
        }

        for (const auto [vertexHandle] : m_impl->sortedNodes)
        {
            const auto nodeHandle = m_impl->graph.get(vertexHandle).node;
            auto* const node = m_impl->nodes.try_find(nodeHandle);

            m_impl->currentNode = node;
            buildState.currentPass = {};

            m_impl->passesPerNode.push_back({
                .passesBegin = u32(m_impl->passes.size()),
                .passesEnd = u32(m_impl->passes.size()),
            });

            auto* const ptr = node->ptr;

            if (node->build)
            {
                OBLO_PROFILE_SCOPE("Build Node");
                OBLO_PROFILE_TAG(node->typeId.name);

                node->build(ptr, buildCtx);

                // Close the last pass build (if any)
                m_impl->end_pass_build(buildState);
            }
        }

        m_impl->currentNode = {};

        if (m_impl->is_recording_metrics() && !m_impl->pendingMetrics.empty())
        {
            OBLO_PROFILE_SCOPE("Build Metrics Recorder");

            m_impl->passesPerNode.push_back({
                .passesBegin = u32(m_impl->passes.size()),
                .passesEnd = u32(m_impl->passes.size()),
            });

            m_impl->pendingMetricsTransfer = buildCtx.transfer_pass();

            for (const auto& pending : m_impl->pendingMetrics)
            {
                buildCtx.acquire(pending.buffer, buffer_access::download);
            }

            m_impl->end_pass_build(buildState);
        }

        m_impl->resourcePool.end_build(gpu);

        auto& textureRegistry = args.rendererPlatform.textureRegistry;

        for (auto& texture : m_impl->bindlessTextures)
        {
            const auto storage = to_storage_handle(texture.texture);
            const auto transientTexture = m_impl->pinStorage.at(storage).transientTexture;

            const auto& t = m_impl->resourcePool.get_transient_texture(transientTexture);
            textureRegistry.set_external_texture(texture.resident, t.handle, texture.state);
        }
    }

    void frame_graph::execute(const frame_graph_execute_args& args)
    {
        OBLO_PROFILE_SCOPE("Frame Graph Execute");

        gpu::gpu_instance& gpu = args.gpu;
        const hptr<gpu::command_buffer> commandBuffer = args.commandBuffer;
        auto& resourcePool = m_impl->resourcePool;

        m_impl->downloadStaging.begin_frame(gpu.get_submit_index());

        for (const auto [storage, poolIndex] : m_impl->transientBuffers)
        {
            const frame_graph_buffer_impl& buf = resourcePool.get_transient_buffer(poolIndex);

            auto& data = m_impl->pinStorage.at(storage);

            // This should only really happen for dynamic pins, we could consider adding some debug check though
            if (!data.data)
            {
                data.data = m_impl->dynamicAllocator.allocate(sizeof(frame_graph_buffer_impl),
                    alignof(frame_graph_buffer_impl));
            }

            new (data.data) frame_graph_buffer_impl{buf};
        }

        if (!m_impl->pendingUploads.empty())
        {
            m_impl->flush_uploads(commandBuffer, args.stagingBuffer);
        }

        // Prepare the download buffers
        for (auto& enqueuedDownload : m_impl->bufferDownloads)
        {
            enqueuedDownload.pendingDownloadId = m_impl->pendingDownloads.size32();
            auto& download = m_impl->pendingDownloads.emplace_back();
            auto& data = m_impl->pinStorage.at(enqueuedDownload.pinStorage);

            frame_graph_buffer_impl* const b = reinterpret_cast<frame_graph_buffer_impl*>(data.data);
            auto staging = m_impl->downloadStaging.stage_allocate(b->size);

            if (!staging)
            {
                OBLO_ASSERT(staging, "Allocation of download buffer failed");
                continue;
            }

            // This only works as long as we submit once per frame
            download.submitIndex = gpu.get_submit_index();
            download.stagedSpan = *staging;
            download.promise.init(get_global_allocator());
        }

        frame_graph_execution_state executionState{
            .gpu = &gpu,
            .commandBuffer = commandBuffer,
        };

        const frame_graph_execute_context executeCtx{*m_impl, executionState, args};

        auto& imageStateTracker = executionState.imageStateTracker;

        for (const auto [resource, transientTextureHandle] : m_impl->transientTextures)
        {
            const auto t = resourcePool.get_transient_texture(transientTextureHandle);

            // TODO: Need to figure out how to get the initial state
            imageStateTracker.add_tracking(t.handle,
                gpu::pipeline_sync_stage::top_of_pipeline,
                gpu::image_resource_state::undefined);

            auto& storage = m_impl->pinStorage.at(resource);

            // Owned textures already don't need to be overwritten, but actual transient textures are created every
            // frame.
            if (!storage.isOwnedTexture)
            {
                new (storage.data) frame_graph_texture_impl{t};
            }
        }

        // Global memory barrier to cover all uploads we just flushed
        gpu.cmd_apply_barriers(commandBuffer,
            {
                .memory = make_span_initializer<gpu::global_memory_barrier>({
                    {
                        .previousPipelines = gpu::pipeline_sync_stage::transfer,
                        .previousAccesses = gpu::memory_access_type::any_write,
                        .nextPipelines = gpu::pipeline_sync_stage::all_commands,
                        .nextAccesses = gpu::memory_access_type::any_read,
                    },
                }),
            });

        frame_graph_barriers barriers{
            .bufferBarriers{dynamic_array<gpu::buffer_memory_barrier>{&m_impl->dynamicAllocator}},
        };

        build_pass_barriers(*m_impl, barriers.bufferBarriers);

        m_impl->barriers = &barriers;

        // We may have some extra hidden nodes (e.g. metrics download) that do not belong to the sorted nodes list, they
        // don't need to be executed, but they may still have passes that get executed after the main execution
        const std::span regularNodePasses = std::span{m_impl->passesPerNode}.subspan(0, m_impl->sortedNodes.size());

        for (const auto& [nodeToExecute, passesPerNode] : zip_range(m_impl->sortedNodes, regularNodePasses))
        {
            const auto nodeHandle = m_impl->graph.get(nodeToExecute.handle).node;
            auto* const node = m_impl->nodes.try_find(nodeHandle);

            m_impl->currentNode = node;
            executionState.currentPass = {};

            if (passesPerNode.passesBegin < passesPerNode.passesEnd)
            {
                // We automatically start the first pass
                m_impl->begin_pass_execution(h32<frame_graph_pass>{passesPerNode.passesBegin}, executionState);
            }

            auto* const ptr = node->ptr;

            if (node->execute)
            {
                OBLO_PROFILE_SCOPE("Execute Node");
                OBLO_PROFILE_TAG(node->typeId.name);
                node->execute(ptr, executeCtx);
            }

            // We want every node to execute all passes in order, the order check is in the begin_pass_execution call
            // Here we check that we got to the last pass
            OBLO_ASSERT(passesPerNode.passesBegin == passesPerNode.passesEnd ||
                executionState.currentPass.value == passesPerNode.passesEnd - 1)
        }

        if (m_impl->is_recording_metrics())
        {
            OBLO_PROFILE_SCOPE("Execute Metrics Recorder");

            async_metrics metrics;

            if (m_impl->pendingMetricsTransfer && executeCtx.begin_pass(m_impl->pendingMetricsTransfer))
            {
                dynamic_array<async_metrics_entry> entries;
                entries.reserve(m_impl->pendingMetrics.size());

                for (const auto& pending : m_impl->pendingMetrics)
                {
                    entries.emplace_back(pending.type, executeCtx.download(pending.buffer));
                }

                metrics.init(std::move(entries));
            }

            m_impl->nextFrameMetrics.set_value(std::move(metrics));
            m_impl->nextFrameMetrics.reset();
            m_impl->pendingMetricsTransfer = {};
            m_impl->pendingMetrics.clear();
        }

        m_impl->barriers = {};
        m_impl->currentNode = {};

        auto& textureRegistry = args.rendererPlatform.textureRegistry;

        for (const auto& texture : m_impl->bindlessTextures)
        {
            textureRegistry.remove(texture.resident);
        }

        m_impl->bindlessTextures.clear();

        m_impl->downloadStaging.end_frame();

        m_impl->flush_downloads(gpu);

        m_impl->finish_frame();
    }

    h32<frame_graph_pass> frame_graph_impl::begin_pass_build(frame_graph_build_state& state, pass_kind passKind)
    {
        end_pass_build(state);

        const auto i = u32(passes.size());
        const h32<frame_graph_pass> passId{i};

        passes.push_back({
            .kind = passKind,
            .textureTransitionBegin = textureTransitions.size32(),
            .bufferUsageBegin = bufferUsages.size32(),
            .bufferDownloadBegin = bufferDownloads.size32(),
        });

        state.currentPass = passId;
        passesPerNode.back().passesEnd = i + 1;

        return passId;
    }

    void frame_graph_impl::end_pass_build(frame_graph_build_state& state)
    {
        if (!state.currentPass)
        {
            return;
        }

        auto& pass = passes[state.currentPass.value];
        pass.textureTransitionEnd = textureTransitions.size32();
        pass.bufferUsageEnd = bufferUsages.size32();
        pass.bufferDownloadEnd = bufferDownloads.size32(),

        state.currentPass = {};
    }

    void frame_graph_impl::begin_pass_execution(h32<frame_graph_pass> passId, frame_graph_execution_state& state) const
    {
        OBLO_ASSERT(passId);

        const auto currentPass = state.currentPass;

        if (currentPass == passId)
        {
            return;
        }

        OBLO_ASSERT(!currentPass || passId.value == currentPass.value + 1);

        const auto& pass = passes[passId.value];

        flags<gpu::pipeline_sync_stage> stages{};

        switch (pass.kind)
        {
        case pass_kind::transfer:
            stages = gpu::pipeline_sync_stage::transfer;
            break;
        case pass_kind::raytracing:
            stages = gpu::pipeline_sync_stage::raytracing;
            break;
        case pass_kind::graphics:
            stages = gpu::pipeline_sync_stage::graphics;
            break;
        }

        // TODO: Can we also prepare these in advance?
        buffered_array<gpu::image_state_transition, 32> imageBarriers;
        imageBarriers.reserve(pass.textureTransitionEnd - pass.textureTransitionBegin);

        for (u32 i = pass.textureTransitionBegin; i != pass.textureTransitionEnd; ++i)
        {
            const auto& textureTransition = textureTransitions[i];

            auto* const impl =
                reinterpret_cast<const frame_graph_texture_impl*>(access_storage(textureTransition.texture));

            const expected transition =
                state.imageStateTracker.add_transition(impl->handle, stages, textureTransition.newState);

            if (transition)
            {
                imageBarriers.push_back(*transition);
            }
        }

        const auto bufferBarriers = std::span{
            barriers->bufferBarriers.data() + pass.bufferBarriersBegin,
            barriers->bufferBarriers.data() + pass.bufferBarriersEnd,
        };

        if (!bufferBarriers.empty() || !imageBarriers.empty())
        {
            state.gpu->cmd_apply_barriers(state.commandBuffer,
                {
                    .buffers = bufferBarriers,
                    .images = imageBarriers,
                });
        }

        state.currentPass = passId;
    }

    void frame_graph_impl::add_metrics_download(const type_id& typeId, pin::buffer b)
    {
        pendingMetrics.emplace_back(typeId, b);
    }

    bool frame_graph_impl::is_recording_metrics() const
    {
        return nextFrameMetrics.is_initialized();
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

    void* frame_graph::try_get_output(h32<frame_graph_subgraph> graph, string_view name, const type_id& typeId)
    {
        auto* const graphPtr = m_impl->subgraphs.try_find(graph);

        if (!graphPtr)
        {
            return nullptr;
        }

        const auto it = graphPtr->outputs.find(name);

        if (it == graphPtr->outputs.end())
        {
            return nullptr;
        }

        const auto& v = m_impl->graph[it->second];

        if (!v.pin)
        {
            return nullptr;
        }

        const auto& pinData = *m_impl->pins.try_find(v.pin);

        auto* storage = pinData.referencedPin ? m_impl->pinStorage.try_find(pinData.referencedPin) : nullptr;

        if (!storage)
        {
            OBLO_ASSERT(pinData.ownedStorage);
            storage = m_impl->pinStorage.try_find(pinData.ownedStorage);
        }

        if (storage->typeDesc.typeId != typeId)
        {
            return nullptr;
        }

        if (!storage->data)
        {
            void* const dataPtr = m_impl->memoryPool.allocate(storage->typeDesc.size, storage->typeDesc.alignment);
            storage->typeDesc.construct(dataPtr);
            storage->data = dataPtr;
        }

        return storage->data;
    }

    void frame_graph::fetch_subgraphs(deque<h32<frame_graph_subgraph>>& outSubgraphs)
    {
        const std::span subgraphs = m_impl->subgraphs.keys();
        outSubgraphs.append(subgraphs.begin(), subgraphs.end());
    }

    void frame_graph::fetch_outputs(h32<frame_graph_subgraph> subgraph,
        deque<frame_graph_output_desc>& outSubgraphOutputs)
    {
        auto* const sg = m_impl->subgraphs.try_find(subgraph);

        if (!sg)
        {
            return;
        }

        for (auto& [name, v] : sg->outputs)
        {
            const auto& vertexData = m_impl->graph[v];

            if (!vertexData.pin)
            {
                OBLO_ASSERT(false);
                continue;
            }

            const auto& pinData = *m_impl->pins.try_find(vertexData.pin);

            OBLO_ASSERT(pinData.ownedStorage);
            auto& storage = *m_impl->pinStorage.try_find(pinData.ownedStorage);

            outSubgraphOutputs.emplace_back(name, storage.typeDesc.typeId);
        }
    }

    future<async_metrics> frame_graph::request_metrics()
    {
        return m_impl->request_metrics();
    }

    void frame_graph::push_empty_event_impl(const type_id& type)
    {
        m_impl->emptyEvents.emplace(type);
    }

    bool frame_graph::has_event_impl(const type_id& type) const
    {
        return m_impl->emptyEvents.contains(type);
    }

    void frame_graph_impl::add_transient_resource(pin::texture handle, h32<transient_texture_resource> transientTexture)
    {
        const auto storage = to_storage_handle(handle);
        transientTextures.emplace_back(storage, transientTexture);
        pinStorage.at(storage).transientTexture = transientTexture;
    }

    void frame_graph_impl::add_resource_transition(pin::texture handle, gpu::image_resource_state newState)
    {
        const auto storage = to_storage_handle(handle);
        textureTransitions.emplace_back(storage, newState);
    }

    h32<transient_texture_resource> frame_graph_impl::find_pool_index(pin::texture handle) const
    {
        const auto storage = to_storage_handle(handle);
        return pinStorage.at(storage).transientTexture;
    }

    h32<transient_buffer_resource> frame_graph_impl::find_pool_index(pin::buffer handle) const
    {
        const auto storage = to_storage_handle(handle);
        return pinStorage.at(storage).transientBuffer;
    }

    void frame_graph_impl::add_transient_buffer(
        pin::buffer handle, h32<transient_buffer_resource> transientBuffer, const staging_buffer_span* upload)
    {
        const auto storage = to_storage_handle(handle);
        transientBuffers.emplace_back(storage, transientBuffer);
        pinStorage.at(storage).transientBuffer = transientBuffer;

        if (upload)
        {
            pendingUploads.emplace_back(storage, *upload);
        }
    }

    void frame_graph_impl::set_buffer_access(pin::buffer handle,
        flags<gpu::pipeline_sync_stage> pipelineStage,
        flags<gpu::memory_access_type> access,
        buffer_access_kind accessKind,
        bool uploadedTo)
    {
        const auto storage = to_storage_handle(handle);

        OBLO_ASSERT(storage);
        OBLO_ASSERT(pinStorage.try_find(storage));

        bufferUsages.push_back({
            .pinStorage = storage,
            .stages = pipelineStage,
            .access = access,
            .accessKind = accessKind,
            .uploadedTo = uploadedTo,
        });
    }

    void frame_graph_impl::add_download(pin::buffer handle)
    {
        auto& bufferDownload = bufferDownloads.emplace_back();
        bufferDownload.pinStorage = to_storage_handle(handle);
    }

    h32<frame_graph_pin_storage> frame_graph_impl::allocate_dynamic_resource_pin()
    {
        const auto [storage, key] = pinStorage.emplace();
        const auto handle = h32<frame_graph_pin_storage>{key};

        dynamicPins.emplace_back(handle);

        return handle;
    }

    h32<frame_graph_pin_storage> frame_graph_impl::create_retained_texture(gpu::gpu_instance& gpu,
        const gpu::image_descriptor& initializer)
    {
        const expected newImage = gpu.create_image(initializer);

        if (!newImage)
        {
            return {};
        }

        const auto [storage, key] = pinStorage.emplace();
        const auto handle = h32<frame_graph_pin_storage>{key};

        frame_graph_texture_impl* const t =
            new (memoryPool.allocate(sizeof(frame_graph_texture_impl), alignof(frame_graph_texture_impl)))
                frame_graph_texture_impl{
                    .handle = *newImage,
                    .descriptor = initializer,
                };

        storage->data = t;
        storage->isOwnedTexture = true;
        storage->transientTexture = resourcePool.add_external_texture(gpu, *newImage);
        storage->typeDesc = frame_graph_data_desc::make<frame_graph_texture_impl>();

        transientTextures.emplace_back(key, storage->transientTexture);
        retainedTextures.emplace(key);

        if (currentNode->subgraph)
        {
            auto& sg = subgraphs.at(currentNode->subgraph);
            sg.dynamicStorage.emplace_back(key);
        }

        return handle;
    }

    void frame_graph_impl::destroy_retained_texture(gpu::gpu_instance& ctx, h32<frame_graph_pin_storage> handle)
    {
        auto* const storage = pinStorage.try_find(handle);

        if (!storage)
        {
            return;
        }

        OBLO_ASSERT(storage->isOwnedTexture);

        auto* const t = static_cast<frame_graph_texture_impl*>(storage->data);

        if (t)
        {
            if (t->handle)
            {
                ctx.destroy_deferred(t->handle, ctx.get_submit_index());
            }

            memoryPool.deallocate(t, sizeof(frame_graph_texture_impl), alignof(frame_graph_texture_impl));
        }

        pinStorage.erase(handle);
        retainedTextures.erase(handle);

        if (currentNode->subgraph)
        {
            auto& sg = subgraphs.at(currentNode->subgraph);

            for (auto it = sg.dynamicStorage.begin(); it != sg.dynamicStorage.end(); ++it)
            {
                if (*it == handle)
                {
                    sg.dynamicStorage.erase_unordered(it);
                    break;
                }
            }
        }
    }

    const frame_graph_node* frame_graph_impl::get_owner_node(pin::buffer buffer) const
    {
        const auto storage = to_storage_handle(buffer);
        const auto owner = pinStorage.at(storage).owner;
        const auto vertexHandle = pins.at(owner).nodeHandle;
        const auto nodeHandle = graph.get(vertexHandle).node;
        return &nodes.at(nodeHandle);
    }

    const frame_graph_node* frame_graph_impl::get_owner_node(pin::texture texture) const
    {
        const auto storage = to_storage_handle(texture);
        const auto owner = pinStorage.at(storage).owner;
        const auto vertexHandle = pins.at(owner).nodeHandle;
        const auto nodeHandle = graph.get(vertexHandle).node;
        return &nodes.at(nodeHandle);
    }

    void frame_graph_impl::reroute(h32<frame_graph_pin_storage> srcStorageHandle,
        h32<frame_graph_pin_storage> dstStorageHandle)
    {
        const auto& srcRouteStorage = pinStorage.at(srcStorageHandle);
        auto& dstRouteStorage = pinStorage.at(dstStorageHandle);

        rerouteStash.emplace_back(dstStorageHandle, dstRouteStorage);

        dstRouteStorage = srcRouteStorage;
    }

    void* frame_graph_impl::access_storage(h32<frame_graph_pin_storage> handle) const
    {
        const auto& storage = pinStorage.at(handle);
        return storage.data;
    }

    void frame_graph_impl::rebuild_runtime(const frame_graph_build_args& args)
    {
        const frame_graph_init_context initCtx{*this, args};

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

                    sortedNodes.push_back({
                        .handle = v,
                    });

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

        // Clear the path to output flags, we will set pins that lead to outputs as active
        for (auto& pin : pinStorage.values())
        {
            pin.hasPathToOutput = false;
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

                // Check if the pin is active, this can be used by nodes to cull some outputs of the node
                // In order to do it we look for outgoing edges to nodes that were marked active (i.e. not culled)
                // Active nodes lead to an output, so we assume the output contributes to that
                OBLO_ASSERT(pin->ownedStorage);
                auto* const ownedStorage = pinStorage.try_find(pin->ownedStorage);

                for (const auto out : graph.get_out_edges(v))
                {
                    const auto& outVertex = graph[out.vertex];

                    OBLO_ASSERT(outVertex.kind == frame_graph_vertex_kind::pin);

                    auto* const dstPin = pins.try_find(outVertex.pin);
                    const auto dstNode = dstPin->nodeHandle;

                    const auto& dstNodeVertex = graph[dstNode];
                    OBLO_ASSERT(dstNodeVertex.kind == frame_graph_vertex_kind::node);

                    if (outVertex.state == frame_graph_vertex_state::enabled)
                    {
                        ownedStorage->hasPathToOutput = true;
                        break;
                    }
                }

                const bool hasIncomingReference = bool{pin->referencedPin};

                if (!hasIncomingReference)
                {
                    if (!ownedStorage->data)
                    {
                        ownedStorage->data =
                            memoryPool.allocate(ownedStorage->typeDesc.size, ownedStorage->typeDesc.alignment);

                        ownedStorage->typeDesc.construct(ownedStorage->data);
                    }

                    pin->referencedPin = pin->ownedStorage;

                    // For data_sink we clear here
                    if (pin->clearDataSink)
                    {
                        pin->clearDataSink(ownedStorage->data);
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

    void frame_graph_impl::flush_uploads(hptr<gpu::command_buffer> commandBuffer, gpu::staging_buffer& stagingBuffer)
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

        vkCmdPipelineBarrier2(std::bit_cast<VkCommandBuffer>(commandBuffer), &beforeDependencyInfo);

        for (const auto& upload : pendingUploads)
        {
            const auto& storage = pinStorage.at(upload.buffer);
            const auto* const b = reinterpret_cast<frame_graph_buffer_impl*>(storage.data);
            stagingBuffer.upload(commandBuffer, upload.source, b->handle, b->offset);
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

        vkCmdPipelineBarrier2(std::bit_cast<VkCommandBuffer>(commandBuffer), &afterDependencyInfo);
    }

    void frame_graph_impl::flush_downloads(gpu::gpu_instance& queueCtx)
    {
        const auto lastFinishedFrame = queueCtx.get_last_finished_submit();

        downloadStaging.notify_finished_frames(lastFinishedFrame);

        u32 numCompletedDownloads = 0;
        bool isFirstDownload = true;

        for (auto& pendingDownload : pendingDownloads)
        {
            if (lastFinishedFrame < pendingDownload.submitIndex)
            {
                break;
            }

            if (isFirstDownload)
            {
                downloadStaging.invalidate_memory_ranges().assert_value();
                isFirstDownload = false;
            }

            const auto totalSize =
                (pendingDownload.stagedSpan.segments[0].end - pendingDownload.stagedSpan.segments[0].begin) +
                (pendingDownload.stagedSpan.segments[1].end - pendingDownload.stagedSpan.segments[1].begin);

            dynamic_array<byte> destination;
            destination.resize_default(totalSize);
            downloadStaging.copy_from(destination, pendingDownload.stagedSpan, 0);

            pendingDownload.promise.set_value(std::move(destination));
            ++numCompletedDownloads;
        }

        if (numCompletedDownloads > 0)
        {
            pendingDownloads.erase(pendingDownloads.begin(), pendingDownloads.begin() + numCompletedDownloads);
        }
    }

    void frame_graph_impl::finish_frame()
    {
        // Re-establish stashed reroutes (in reverse order just in case it matters)
        for (const auto& [handle, value] : reverse_range(rerouteStash))
        {
            pinStorage.at(handle) = value;
        }

        for (auto& h : dynamicPins)
        {
            const auto& storage = pinStorage.at(h);
            free_pin_storage(h, storage, true);
            pinStorage.erase(h);
        }

        textureTransitions.clear();
        transientTextures.clear();
        transientBuffers.clear();
        dynamicPins.clear();
        emptyEvents.clear();
        rerouteStash.clear();
        bufferDownloads.clear();

        ++frameCounter;

        globalTLAS = {};
    }

    future<async_metrics> frame_graph_impl::request_metrics()
    {
        if (!nextFrameMetrics.is_initialized())
        {
            nextFrameMetrics.init();
        }

        return future{nextFrameMetrics};
    }

    void frame_graph_impl::free_pin_storage(
        h32<frame_graph_pin_storage> key, const frame_graph_pin_storage& storage, bool isFrameAllocated)
    {
        if (storage.data)
        {
            if (storage.isOwnedTexture)
            {
                const auto* const t = static_cast<frame_graph_texture_impl*>(storage.data);
                pendingTexturesToFree.emplace_back(*t);
                retainedTextures.erase(key);
            }

            if (storage.typeDesc.destruct)
            {
                storage.typeDesc.destruct(storage.data);
            }

            // These pointers should always come from the frame allocator, no need to delete them
            OBLO_ASSERT(isFrameAllocated == dynamicAllocator.contains(storage.data));

            if (!isFrameAllocated)
            {
                memoryPool.deallocate(storage.data, storage.typeDesc.size, storage.typeDesc.alignment);
            }
        }
    }

    void frame_graph_impl::free_pending_textures(gpu::gpu_instance& ctx)
    {
        for (const auto& t : pendingTexturesToFree)
        {
            if (t.handle)
            {
                ctx.destroy_deferred(t.handle, ctx.get_submit_index());
            }
        }

        pendingTexturesToFree.clear();
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