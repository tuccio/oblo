#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/gpu_allocator.hpp>
#include <oblo/vulkan/texture.hpp>

#include <vector>

namespace oblo::vk
{
    class vulkan_context;

    struct lifetime_range
    {
        u16 begin;
        u16 end;
    };

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

        u32 add(const image_initializer& initializer, lifetime_range range);

        u32 add_buffer(u32 size, VkBufferUsageFlags usage);

        void add_usage(u32 poolIndex, VkImageUsageFlags usage);
        void add_buffer_usage(u32 poolIndex, VkBufferUsageFlags usage);

        texture get_texture(u32 id) const;
        buffer get_buffer(u32 id) const;

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
        std::vector<texture_resource> m_textureResources;
        std::vector<texture_resource> m_lastFrameTextureResources;

        dynamic_array<buffer_resource> m_bufferResources;

        dynamic_array<buffer_pool> m_bufferPools;

        VmaAllocation m_allocation{};
        VmaAllocation m_lastFrameAllocation{};
        VkMemoryRequirements m_requirements{};
    };
}