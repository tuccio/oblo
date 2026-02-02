#pragma once

#include <oblo/vulkan/gpu_temporary_aliases.hpp>

#include <vulkan/vulkan_core.h>

namespace oblo::vk
{
    class staging_buffer;
    class vulkan_context;

    struct buffer;

    class dynamic_buffer
    {
    public:
        dynamic_buffer() = default;
        dynamic_buffer(const dynamic_buffer&) = delete;
        dynamic_buffer(dynamic_buffer&&) noexcept = delete;
        ~dynamic_buffer();

        dynamic_buffer& operator=(const dynamic_buffer&) = delete;
        dynamic_buffer& operator=(dynamic_buffer&&) noexcept = delete;

        void init(vulkan_context& ctx, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryPropertyFlags);
        void shutdown();

        void clear();
        void clear_and_shrink();

        void resize(VkCommandBuffer cmd, u32 size);
        void resize_discard(u32 size);

        void reserve(VkCommandBuffer cmd, u32 size);
        void reserve_discard(u32 size);

        buffer get_buffer() const;

        u32 get_capacity() const;

        u32 get_used_bytes() const;

    private:
        void reserve_impl(VkCommandBuffer cmd, u32 size);

    private:
        vulkan_context* m_ctx{};
        allocated_buffer m_buffer{};
        u32 m_capacity{};
        u32 m_usedBytes{};
        VkBufferUsageFlags m_usage{};
        VkMemoryPropertyFlags m_memoryPropertyFlags{};
    };
}