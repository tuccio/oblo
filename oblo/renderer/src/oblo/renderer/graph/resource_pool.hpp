#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/gpu/forward.hpp>
#include <oblo/gpu/structs.hpp>
#include <oblo/gpu/vulkan/gpu_allocator.hpp>
#include <oblo/renderer/platform/renderer_platform.hpp>

#include <unordered_map>

namespace oblo
{
    enum class buffer_access_kind : u8;

    struct lifetime_range
    {
        u32 begin;
        u32 end;
    };

    struct transient_texture_resource;
    struct transient_buffer_resource;

    struct stable_texture_resource;
    struct stable_buffer_resource;

    class resource_pool
    {
    public:
        resource_pool();
        resource_pool(const resource_pool&) = delete;
        resource_pool(resource_pool&&) noexcept = delete;
        ~resource_pool();

        resource_pool& operator=(const resource_pool&) = delete;
        resource_pool& operator=(resource_pool&&) noexcept = delete;

        bool init(gpu::gpu_queue_context& ctx);
        void shutdown(gpu::gpu_queue_context& ctx);

        void begin_build();
        void end_build(gpu::gpu_queue_context& ctx);

        h32<transient_texture_resource> add_transient_texture(
            const gpu::image_descriptor& initializer, lifetime_range range, h32<stable_texture_resource> stableId);

        h32<transient_texture_resource> add_external_texture(const frame_graph_texture& texture);

        h32<transient_buffer_resource> add_transient_buffer(
            u32 size, flags<gpu::buffer_usage> usage, h32<stable_buffer_resource> stableId);

        void add_transient_texture_usage(h32<transient_texture_resource> transientTexture, gpu::texture_usage usage);
        void add_transient_buffer_usage(h32<transient_buffer_resource> transientBuffer, gpu::buffer_usage usage);

        frame_graph_texture get_transient_texture(h32<transient_texture_resource> id) const;
        frame_graph_buffer get_transient_buffer(h32<transient_buffer_resource> id) const;

        bool is_stable(h32<transient_buffer_resource> id) const;

        u32 get_frames_alive_count(h32<transient_texture_resource> id) const;
        u32 get_frames_alive_count(h32<transient_buffer_resource> id) const;

        const gpu::image_descriptor& get_initializer(h32<transient_texture_resource> id) const;

        void fetch_buffer_tracking(h32<transient_buffer_resource> id,
            VkPipelineStageFlags2* stages,
            VkAccessFlags2* access,
            buffer_access_kind* accessKind) const;

        void store_buffer_tracking(h32<transient_buffer_resource> id,
            VkPipelineStageFlags2 stages,
            VkAccessFlags2 access,
            buffer_access_kind accessKind);

    private:
        struct buffer_resource;
        struct texture_resource;

        struct buffer_pool;

    private:
        void free_last_frame_resources(gpu::gpu_queue_context& ctx);

        void create_textures(gpu::vk::vulkan_instance& gpu);
        void create_buffers(gpu::vk::vulkan_instance& gpu);

        void acquire_from_pool(gpu::vk::vulkan_instance& gpu, texture_resource& resource);
        void acquire_from_pool(gpu::vk::vulkan_instance& gpu, buffer_resource& resource);

        void free_stable_textures(gpu::gpu_queue_context& ctx, u32 unusedFor);
        void free_stable_buffers(gpu::gpu_queue_context& ctx, u32 unusedFor);

    private:
        struct stable_texture_key
        {
            h32<stable_texture_resource> stableId;
            gpu::image_descriptor descriptor;

            bool operator==(const stable_texture_key& rhs) const;
        };

        struct stable_texture_key_hash
        {
            usize operator()(const stable_texture_key& key) const;
        };

        struct stable_buffer_key
        {
            h32<stable_buffer_resource> stableId;
            flags<gpu::buffer_usage> usage;
            u64 size;

            bool operator==(const stable_buffer_key& rhs) const = default;
        };

        struct stable_buffer_key_hash
        {
            usize operator()(const stable_buffer_key& key) const;
        };

        struct stable_texture;
        struct stable_buffer;

        using stable_textures_map = std::unordered_map<stable_texture_key, stable_texture, stable_texture_key_hash>;
        using stable_buffers_map = std::unordered_map<stable_buffer_key, stable_buffer, stable_buffer_key_hash>;

    private:
        dynamic_array<texture_resource> m_textureResources;

        stable_textures_map m_stableTextures;
        stable_buffers_map m_stableBuffers;

        dynamic_array<buffer_resource> m_bufferResources;

        dynamic_array<buffer_pool> m_bufferPools;

        h32<gpu::image_pool> m_currentFramePool{};
        h32<gpu::image_pool> m_lastFramePool{};

        u32 m_frame{};
    };
}