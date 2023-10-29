#pragma once

#include <oblo/vulkan/allocator.hpp>
#include <oblo/vulkan/monotonic_gbu_buffer.hpp>
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
        buffer add_uniform_buffer(vulkan_context& ctx, u32 size);

        void add_usage(u32 poolIndex, VkImageUsageFlags usage);

        texture get_texture(u32 id) const;

    private:
        struct texture_resource;

    private:
        void free_last_frame_resources(vulkan_context& ctx);

    private:
        u32 m_graphBegin{0};
        std::vector<texture_resource> m_textureResources;
        std::vector<texture_resource> m_lastFrameTextureResources;

        monotonic_gpu_buffer m_uniformBuffersPool;

        VmaAllocation m_allocation{};
        VmaAllocation m_lastFrameAllocation{};
        VkMemoryRequirements m_requirements{};
    };
}