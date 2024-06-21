#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/frame_allocator.hpp>
#include <oblo/core/graph/directed_graph.hpp>
#include <oblo/core/handle_flat_pool_map.hpp>
#include <oblo/core/hash.hpp>
#include <oblo/core/types.hpp>
#include <oblo/vulkan/graph/frame_graph_node_desc.hpp>
#include <oblo/vulkan/graph/frame_graph_template.hpp>
#include <oblo/vulkan/graph/frame_graph_vertex_kind.hpp>
#include <oblo/vulkan/graph/pins.hpp>
#include <oblo/vulkan/graph/resource_pool.hpp>
#include <oblo/vulkan/staging_buffer.hpp>

#include <iosfwd>
#include <memory_resource>
#include <string>
#include <unordered_map>

#include <vulkan/vulkan_core.h>

namespace oblo::vk
{
    struct frame_graph_subgraph;
    struct frame_graph_vertex;

    using frame_graph_topology = directed_graph<frame_graph_vertex>;

    struct frame_graph_buffer_usage
    {
        VkPipelineStageFlags2 stages;
        VkAccessFlags2 access;
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
        bool initialized;
        bool markedForRemoval;

        h32_flat_extpool_dense_map<frame_graph_pin_storage, frame_graph_buffer_usage> bufferUsages;
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
        bool forwardAccess;
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

    using name_to_vertex_map =
        std::unordered_map<std::string, frame_graph_topology::vertex_handle, transparent_string_hash, std::equal_to<>>;

    struct frame_graph_subgraph
    {
        flat_dense_map<frame_graph_template_vertex_handle, frame_graph_topology::vertex_handle> templateToInstanceMap;
        name_to_vertex_map inputs;
        name_to_vertex_map outputs;
    };

    struct frame_graph_node_to_execute
    {
        frame_graph_node* node;
        frame_graph_topology::vertex_handle handle;
    };

    struct frame_graph_impl
    {

    public: // Topology
        frame_graph_topology graph;
        h32_flat_pool_dense_map<frame_graph_node> nodes;
        h32_flat_pool_dense_map<frame_graph_pin> pins;
        h32_flat_pool_dense_map<frame_graph_pin_storage> pinStorage;
        h32_flat_pool_dense_map<frame_graph_subgraph> subgraphs;

        std::pmr::unsynchronized_pool_resource memoryPool;

    public: // Runtime
        frame_allocator dynamicAllocator;
        resource_manager* resourceManager{};

        dynamic_array<frame_graph_node_to_execute> sortedNodes;
        h32_flat_pool_dense_map<frame_graph_texture> textures;
        h32_flat_pool_dense_map<frame_graph_buffer> buffers;

        dynamic_array<frame_graph_node_transitions> nodeTransitions;
        dynamic_array<frame_graph_texture_transition> textureTransitions;
        dynamic_array<frame_graph_texture> transientTextures;
        dynamic_array<frame_graph_buffer> transientBuffers;
        dynamic_array<frame_graph_pending_upload> pendingUploads;

        dynamic_array<h32<frame_graph_pin_storage>> dynamicPins;

        resource_pool resourcePool;

        frame_graph_node* currentNode{};

    public: // Internals for frame graph execution
        void rebuild_runtime(renderer& renderer);
        void flush_uploads(VkCommandBuffer commandBuffer, staging_buffer& stagingBuffer);
        void finish_frame();

    public: // API for contexts
        void* access_storage(h32<frame_graph_pin_storage> handle) const;

        void add_transient_resource(resource<texture> handle, u32 poolIndex);
        void add_resource_transition(resource<texture> handle, VkImageLayout target);

        u32 find_pool_index(resource<texture> handle) const;
        u32 find_pool_index(resource<buffer> handle) const;

        void add_transient_buffer(resource<buffer> handle, u32 poolIndex, const staging_buffer_span* upload);
        void set_buffer_access(resource<buffer> handle, VkPipelineStageFlags2 pipelineStage, VkAccessFlags2 access);

        // This is called in assert by executors, to check that we declared the upload when building
        bool can_exec_time_upload(resource<buffer> handle) const;

        h32<frame_graph_pin_storage> allocate_dynamic_resource_pin();

    public: // Utility
        void free_pin_storage(const frame_graph_pin_storage& storage, bool isFrameAllocated);

        [[maybe_unused]] void write_dot(std::ostream& os) const;
    };
}