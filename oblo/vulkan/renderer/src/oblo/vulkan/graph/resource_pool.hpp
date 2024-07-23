#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/gpu_allocator.hpp>
#include <oblo/vulkan/texture.hpp>

namespace oblo::vk
{
    class vulkan_context;

    struct lifetime_range
    {
        u32 begin;
        u32 end;

        constexpr bool operator==(const lifetime_range&) const = default;

        static constexpr lifetime_range infinite();
    };

    struct transient_texture_resource;
    struct transient_buffer_resource;

    constexpr lifetime_range lifetime_range::infinite()
    {
        return {~u32{}, ~u32{}};
    }

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

        void begin_graph();
        void end_graph();

        h32<transient_texture_resource> add_transient_texture(const image_initializer& initializer,
            lifetime_range range);

        h32<transient_buffer_resource> add_transient_buffer(u32 size, VkBufferUsageFlags usage);

        void add_transient_texture_usage(h32<transient_texture_resource> poolIndex, VkImageUsageFlags usage);
        void add_transient_buffer_usage(h32<transient_buffer_resource> poolIndex, VkBufferUsageFlags usage);

        texture get_transient_texture(h32<transient_texture_resource> id) const;
        buffer get_transient_buffer(h32<transient_buffer_resource> id) const;

    private:
        struct buffer_resource;
        struct texture_resource;

        struct buffer_pool;

    private:
        void free_last_frame_resources(vulkan_context& ctx);

        void create_textures(vulkan_context& ctx);
        void create_buffers(vulkan_context& ctx);

    private:
        u32 m_graphBegin{0};
        dynamic_array<texture_resource> m_transientTextures;
        dynamic_array<texture_resource> m_lastFrameTransientTextures;

        dynamic_array<buffer_resource> m_bufferResources;

        dynamic_array<buffer_pool> m_bufferPools;

        VmaAllocation m_allocation{};
        VmaAllocation m_lastFrameAllocation{};
        VkMemoryRequirements m_requirements{};
    };
}