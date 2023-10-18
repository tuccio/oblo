#pragma once

#include <oblo/vulkan/allocator.hpp>
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

        void begin_build(u64 frameIndex);
        void end_build(const vulkan_context& ctx);

        void begin_graph();
        void end_graph();

        u32 add(const image_initializer& initializer, lifetime_range range);

        texture get_texture(u32 id) const;

    private:
        struct texture_resource;

    private:
        u32 m_graphBegin{0};
        std::vector<texture_resource> m_textureResources;

        VmaAllocation m_allocation{};
        VkMemoryRequirements m_requirements{};
    };
}