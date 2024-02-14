#pragma once

#include <oblo/vulkan/dynamic_buffer.hpp>

#include <span>

namespace oblo::vk
{
    class dynamic_array_buffer
    {
    public:
        void init(
            vulkan_context& ctx, u32 elementSize, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryPropertyFlags);

        template <typename T>
        void init(vulkan_context& ctx, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryPropertyFlags);

        void shutdown();

        void clear_and_shrink();

        buffer get_buffer() const;

        buffer get_element_at(u32 index) const;

        u32 get_used_bytes() const;

        u32 get_elements_count() const;

        void resize(VkCommandBuffer cmd, u32 elementsCount);

    private:
        void grow(VkCommandBuffer cmd, u32 newByteSize);

    private:
        dynamic_buffer m_buffer;
        u32 m_elementSize{};
    };

    template <typename T>
    void dynamic_array_buffer::init(
        vulkan_context& ctx, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryPropertyFlags)
    {
        init(ctx, sizeof(T), usage, memoryPropertyFlags);
    }
}