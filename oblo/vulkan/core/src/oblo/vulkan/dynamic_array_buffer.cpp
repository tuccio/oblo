#include <oblo/vulkan/dynamic_array_buffer.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/staging_buffer.hpp>

namespace oblo::vk
{
    void dynamic_array_buffer::init(
        vulkan_context& ctx, u32 elementSize, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryPropertyFlags)
    {
        m_buffer.init(ctx, usage, memoryPropertyFlags);
        m_elementSize = elementSize;
    }

    void dynamic_array_buffer::shutdown()
    {
        m_buffer.shutdown();
    }

    void dynamic_array_buffer::clear_and_shrink()
    {
        m_buffer.clear_and_shrink();
    }

    u32 dynamic_array_buffer::get_elements_count() const
    {
        return get_used_bytes() / m_elementSize;
    }

    void dynamic_array_buffer::resize(VkCommandBuffer cmd, u32 elementsCount)
    {
        grow(cmd, m_elementSize * elementsCount);
    }

    buffer dynamic_array_buffer::get_buffer() const
    {
        return m_buffer.get_buffer();
    }

    buffer dynamic_array_buffer::get_element_at(u32 index) const
    {
        auto b = m_buffer.get_buffer();
        const auto offset = index * m_elementSize;
        OBLO_ASSERT(offset + m_elementSize <= b.size);
        b.offset += offset;
        b.size = m_elementSize;
        return b;
    }

    u32 dynamic_array_buffer::get_used_bytes() const
    {
        return m_buffer.get_used_bytes();
    }

    void dynamic_array_buffer::grow(VkCommandBuffer cmd, u32 newByteSize)
    {
        OBLO_ASSERT(newByteSize % m_elementSize == 0);

        const u32 capacity = m_buffer.get_capacity();

        if (newByteSize > capacity)
        {
            constexpr u32 minElementsCount = 11;
            constexpr f32 growthFactor = 1.5f;

            const u32 oldElementsCount = get_elements_count();

            const u32 finalByteSize =
                max(m_elementSize * min(u32(growthFactor * oldElementsCount + .5f), minElementsCount), newByteSize);
            m_buffer.reserve(cmd, finalByteSize);
        }

        m_buffer.resize(cmd, newByteSize);
    }
}