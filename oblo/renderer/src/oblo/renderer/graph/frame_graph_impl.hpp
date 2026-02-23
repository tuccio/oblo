#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/frame_allocator.hpp>
#include <oblo/core/graph/directed_graph.hpp>
#include <oblo/core/handle_flat_pool_map.hpp>
#include <oblo/core/handle_flat_pool_set.hpp>
#include <oblo/core/hash.hpp>
#include <oblo/core/random_generator.hpp>
#include <oblo/core/string/transparent_string_hash.hpp>
#include <oblo/core/types.hpp>
#include <oblo/gpu/staging_buffer.hpp>
#include <oblo/renderer/data/async_download.hpp>
#include <oblo/renderer/draw/pass_manager.hpp>
#include <oblo/renderer/graph/frame_graph_node_desc.hpp>
#include <oblo/renderer/graph/frame_graph_template.hpp>
#include <oblo/renderer/graph/frame_graph_vertex_kind.hpp>
#include <oblo/renderer/graph/image_layout_tracker.hpp>
#include <oblo/renderer/graph/pins.hpp>
#include <oblo/renderer/graph/resource_pool.hpp>

#include <iosfwd>
#include <memory_resource>
#include <unordered_map>
#include <unordered_set>

#include <vulkan/vulkan_core.h>

namespace oblo
{
    class async_metrics;
}

namespace oblo
{
    struct frame_graph_subgraph;
    struct frame_graph_vertex;

    using frame_graph_topology = directed_graph<frame_graph_vertex>;

    enum class buffer_access_kind : u8
    {
        read,
        write,
    };

    struct frame_graph_buffer_usage
    {
        h32<frame_graph_pin_storage> pinStorage;
        VkPipelineStageFlags2 stages;
        VkAccessFlags2 access;
        buffer_access_kind accessKind;
        bool uploadedTo;
    };

    struct frame_graph_buffer_download
    {
        h32<frame_graph_pin_storage> pinStorage;
        u32 pendingDownloadId;
    };

    struct frame_graph_node
    {
        void* ptr;
        frame_graph_build_fn build;
        frame_graph_execute_fn execute;
        frame_graph_init_fn init;
        frame_graph_destruct_fn destruct;
        type_id typeId;
        u32 size;
        u32 alignment;
        h32<frame_graph_subgraph> subgraph;
        bool initialized;
        bool markedForRemoval;
    };

    struct frame_graph_pin
    {
        h32<frame_graph_pin_storage> ownedStorage;
        h32<frame_graph_pin_storage> referencedPin;
        frame_graph_topology::vertex_handle nodeHandle;
        u32 pinMemberOffset;
        frame_graph_clear_fn clearDataSink;
    };

    struct frame_graph_pin_storage
    {
        void* data;

        // These handles are only valid during a frame for transient resources
        h32<transient_buffer_resource> transientBuffer;
        h32<transient_texture_resource> transientTexture;

        frame_graph_data_desc typeDesc;

        // The handle of the pin that owns the storage
        h32<frame_graph_pin> owner;

        // Whether or not the pin leads to a frame graph output. Updated every frame.
        bool hasPathToOutput;

        // Owned textures, e.g. retained textures. We don't really need this flag, it's more for debugging purposes.
        bool isOwnedTexture;
    };

    enum class frame_graph_vertex_state : u8
    {
        enabled,
        disabled,
        unvisited,
    };

    struct frame_graph_vertex
    {
        frame_graph_vertex_kind kind;
        frame_graph_vertex_state state;
        h32<frame_graph_node> node;
        h32<frame_graph_pin> pin;

#ifdef OBLO_DEBUG
        type_id debugTypeId;
#endif
    };

    struct frame_graph_node_passes
    {
        u32 firstPass;
        u32 passesCount;
    };

    struct frame_graph_texture_transition
    {
        h32<frame_graph_pin_storage> texture;
        texture_access usage;
    };

    struct frame_graph_buffer_barrier
    {
        h32<frame_graph_pin_storage> buffer;
        VkPipelineStageFlags2 pipelineStage;
        VkAccessFlags2 access;
        bool forwardAccess;
    };

    struct frame_graph_texture
    {
        h32<frame_graph_pin_storage> texture;
        h32<transient_texture_resource> poolIndex;
    };

    struct frame_graph_buffer
    {
        h32<frame_graph_pin_storage> buffer;
        h32<transient_buffer_resource> poolIndex;
    };

    struct frame_graph_node_transitions
    {
        u32 firstTextureTransition;
        u32 lastTextureTransition;
    };

    struct frame_graph_pending_upload
    {
        h32<frame_graph_pin_storage> buffer;
        gpu::staging_buffer_span source;
    };

    struct frame_graph_pending_download
    {
        u64 submitIndex;
        gpu::staging_buffer_span stagedSpan;
        async_download_promise promise;
    };

    using name_to_vertex_map =
        std::unordered_map<string, frame_graph_topology::vertex_handle, transparent_string_hash, std::equal_to<>>;

    struct frame_graph_subgraph
    {
        dynamic_array<frame_graph_topology::vertex_handle> vertices;

        // Nodes might create storage at runtime for buffers or textures they want to retain.
        // These are owned by the subgraph, if the subgraph is removed, these have to be cleaned up.
        dynamic_array<h32<frame_graph_pin_storage>> dynamicStorage;

        name_to_vertex_map inputs;
        name_to_vertex_map outputs;
    };

    struct frame_graph_bindless_texture
    {
        h32<resident_texture> resident;
        pin::texture texture;
        texture_access usage;
    };

    struct frame_graph_node_to_execute
    {
        frame_graph_topology::vertex_handle handle;
    };

    struct frame_graph_pass
    {
        pass_kind kind;

        u32 textureTransitionBegin;
        u32 textureTransitionEnd;

        u32 bufferUsageBegin;
        u32 bufferUsageEnd;

        u32 bufferBarriersBegin;
        u32 bufferBarriersEnd;

        u32 bufferDownloadBegin;
        u32 bufferDownloadEnd;

        union {
            h32<compute_pipeline> computePipeline;
            h32<render_pipeline> renderPipeline;
            h32<raytracing_pipeline> raytracingPipeline;
        };
    };

    struct frame_graph_passes_per_node
    {
        u32 passesBegin;
        u32 passesEnd;
    };

    struct frame_graph_barriers
    {
        dynamic_array<VkBufferMemoryBarrier2> bufferBarriers;
        dynamic_array<VkImageMemoryBarrier2> imageBarriers;
    };

    struct frame_graph_pin_reroute
    {
        h32<frame_graph_pin_storage> storageHandle;
        frame_graph_pin_storage value;
    };

    struct frame_graph_metrics
    {
        type_id type;
        pin::buffer buffer;
    };

    struct frame_graph_retained_texture_desc
    {
        gpu::image_descriptor initializer;
        h32<frame_graph_pin_storage> pin;
    };

    struct frame_graph_impl
    {
    public: // Topology
        frame_graph_topology graph;
        h32_flat_pool_dense_map<frame_graph_node> nodes;
        h32_flat_pool_dense_map<frame_graph_pin> pins;
        h32_flat_pool_dense_map<frame_graph_pin_storage> pinStorage;
        h32_flat_pool_dense_map<frame_graph_subgraph> subgraphs;

        // The pin storages of our retained textures, these are added at runtime but they stay alive
        h32_flat_extpool_dense_set<frame_graph_pin_storage> retainedTextures;

        std::pmr::unsynchronized_pool_resource memoryPool;

    public: // Runtime
        frame_allocator dynamicAllocator;
        gpu_info gpuInfo;

        gpu::staging_buffer downloadStaging;

        dynamic_array<frame_graph_node_to_execute> sortedNodes;

        h32_flat_pool_dense_map<frame_graph_texture> textures;
        h32_flat_pool_dense_map<frame_graph_buffer> buffers;

        dynamic_array<frame_graph_node_passes> nodePasses;
        dynamic_array<frame_graph_texture_transition> textureTransitions;
        dynamic_array<frame_graph_buffer_usage> bufferUsages;
        dynamic_array<frame_graph_texture> transientTextures;
        dynamic_array<frame_graph_buffer> transientBuffers;
        deque<frame_graph_buffer_download> bufferDownloads;
        dynamic_array<frame_graph_pending_upload> pendingUploads;

        deque<frame_graph_pass> passes;
        dynamic_array<frame_graph_passes_per_node> passesPerNode;

        dynamic_array<h32<frame_graph_pin_storage>> dynamicPins;
        dynamic_array<frame_graph_bindless_texture> bindlessTextures;

        deque<frame_graph_pending_download> pendingDownloads;
        deque<frame_graph_texture_impl> pendingTexturesToFree;

        deque<frame_graph_pin_reroute> rerouteStash;

        promise<async_metrics> nextFrameMetrics;
        deque<frame_graph_metrics> pendingMetrics;
        h32<transfer_pass_instance> pendingMetricsTransfer{};

        resource_pool resourcePool;

        frame_graph_barriers* barriers{};
        frame_graph_node* currentNode{};

        random_generator rng;

        u32 frameCounter{};

        // Used to send signals to the frame graph (e.g. reset an effect)
        std::unordered_set<type_id> emptyEvents;

        // Temporary until we make the acceleration structure a proper resource
        VkAccelerationStructureKHR globalTLAS{};

    public: // Internals for frame graph execution
        void mark_active_nodes();
        void rebuild_runtime(const frame_graph_build_args& args);
        void flush_uploads(hptr<gpu::command_buffer> commandBuffer, gpu::staging_buffer& stagingBuffer);
        void flush_downloads(gpu::gpu_instance& queueCtx);
        void finish_frame();

        future<async_metrics> request_metrics();

    public: // API for contexts
        void* access_storage(h32<frame_graph_pin_storage> handle) const;

        void add_transient_resource(pin::texture handle, h32<transient_texture_resource> transientTexture);
        void add_resource_transition(pin::texture handle, texture_access usage);

        h32<transient_texture_resource> find_pool_index(pin::texture handle) const;
        h32<transient_buffer_resource> find_pool_index(pin::buffer handle) const;

        void add_transient_buffer(
            pin::buffer handle, h32<transient_buffer_resource> transientBuffer, const gpu::staging_buffer_span* upload);

        void set_buffer_access(pin::buffer handle,
            VkPipelineStageFlags2 pipelineStage,
            VkAccessFlags2 access,
            buffer_access_kind,
            bool uploadedTo);

        void add_download(pin::buffer handle);

        h32<frame_graph_pin_storage> allocate_dynamic_resource_pin();

        h32<frame_graph_pin_storage> create_retained_texture(gpu::gpu_instance& ctx,
            const gpu::image_descriptor& initializer);

        void destroy_retained_texture(gpu::gpu_instance& ctx, h32<frame_graph_pin_storage> handle);

        const frame_graph_node* get_owner_node(pin::buffer buffer) const;
        const frame_graph_node* get_owner_node(pin::texture texture) const;

        void reroute(h32<frame_graph_pin_storage> oldRoute, h32<frame_graph_pin_storage> newRoute);

        h32<frame_graph_pass> begin_pass_build(frame_graph_build_state& state, pass_kind passKind);
        void end_pass_build(frame_graph_build_state& state);

        void begin_pass_execution(h32<frame_graph_pass> pass, frame_graph_execution_state& state) const;

        void add_metrics_download(const type_id& typeId, pin::buffer b);
        bool is_recording_metrics() const;

    public: // Utility
        void free_pin_storage(
            h32<frame_graph_pin_storage> key, const frame_graph_pin_storage& storage, bool isFrameAllocated);

        void free_pending_textures(gpu::gpu_instance& ctx);

        [[maybe_unused]] void write_dot(std::ostream& os) const;
    };

    struct frame_graph_build_state
    {
        h32<frame_graph_pass> currentPass;
    };

    struct frame_graph_execution_state
    {
        hptr<gpu::command_buffer> commandBuffer{};
        h32<frame_graph_pass> currentPass;
        image_layout_tracker imageLayoutTracker;
        pass_kind passKind;
        const base_pipeline* basePipeline{};

        union {
            compute_pass_context computeCtx;
            render_pass_context renderCtx;
            raytracing_pass_context rtCtx;
        };
    };
}