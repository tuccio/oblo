#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/gpu_temporary_aliases.hpp>
#include <oblo/vulkan/texture.hpp>

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

        bool init(vulkan_context& ctx);
        void shutdown(vulkan_context& ctx);

        void begin_build();
        void end_build(vulkan_context& ctx);

        h32<transient_texture_resource> add_transient_texture(
            const image_initializer& initializer, lifetime_range range, h32<stable_texture_resource> stableId);

        h32<transient_texture_resource> add_external_texture(const texture& texture);

        h32<transient_buffer_resource> add_transient_buffer(
            u32 size, VkBufferUsageFlags usage, h32<stable_buffer_resource> stableId);

        void add_transient_texture_usage(h32<transient_texture_resource> transientTexture, VkImageUsageFlags usage);
        void add_transient_buffer_usage(h32<transient_buffer_resource> transientBuffer, VkBufferUsageFlags usage);

        texture get_transient_texture(h32<transient_texture_resource> id) const;
        buffer get_transient_buffer(h32<transient_buffer_resource> id) const;

        bool is_stable(h32<transient_buffer_resource> id) const;

        u32 get_frames_alive_count(h32<transient_texture_resource> id) const;
        u32 get_frames_alive_count(h32<transient_buffer_resource> id) const;

        const image_initializer& get_initializer(h32<transient_texture_resource> id) const;

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
        void free_last_frame_resources(vulkan_context& ctx);

        void create_textures(vulkan_context& ctx);
        void create_buffers(vulkan_context& ctx);

        void acquire_from_pool(vulkan_context& ctx, texture_resource& resource);
        void acquire_from_pool(vulkan_context& ctx, buffer_resource& resource);

        void free_stable_textures(vulkan_context& ctx, u32 unusedFor);
        void free_stable_buffers(vulkan_context& ctx, u32 unusedFor);

    private:
        struct stable_texture_key
        {
            h32<stable_texture_resource> stableId;
            image_initializer initializer;

            bool operator==(const stable_texture_key& rhs) const;
        };

        struct stable_texture_key_hash
        {
            usize operator()(const stable_texture_key& key) const;
        };

        struct stable_buffer_key
        {
            h32<stable_buffer_resource> stableId;
            VkBufferUsageFlags usage;
            u32 size;

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
        dynamic_array<texture_resource> m_lastFrameTransientTextures;

        stable_textures_map m_stableTextures;
        stable_buffers_map m_stableBuffers;

        dynamic_array<buffer_resource> m_bufferResources;

        dynamic_array<buffer_pool> m_bufferPools;

        VmaAllocation m_allocation{};
        VmaAllocation m_lastFrameAllocation{};

        u32 m_frame{};
    };
}