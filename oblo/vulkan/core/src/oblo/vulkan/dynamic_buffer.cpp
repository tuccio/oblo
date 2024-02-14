#include <oblo/vulkan/dynamic_buffer.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/error.hpp>
#include <oblo/vulkan/vulkan_context.hpp>

namespace oblo::vk
{
    dynamic_buffer::~dynamic_buffer()
    {
        OBLO_ASSERT(m_buffer.buffer == nullptr, "Shutdown should be explicit");
    }

    void dynamic_buffer::init(vulkan_context& ctx, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryPropertyFlags)
    {
        m_ctx = &ctx;
        m_buffer = {};
        m_capacity = 0;
        m_usedBytes = 0;
        m_usage = usage;
        m_memoryPropertyFlags = memoryPropertyFlags;
    }

    void dynamic_buffer::shutdown()
    {
        if (m_buffer.buffer)
        {
            m_ctx->destroy_immediate(m_buffer.buffer);
            m_ctx->destroy_immediate(m_buffer.allocation);

            m_buffer = {};
            m_capacity = 0;
            m_usedBytes = {};
        }

        m_ctx = {};
    }

    void dynamic_buffer::clear_and_shrink()
    {
        if (m_buffer.buffer)
        {
            const auto submitIndex = m_ctx->get_submit_index();

            m_ctx->destroy_deferred(m_buffer.buffer, submitIndex);
            m_ctx->destroy_deferred(m_buffer.allocation, submitIndex);

            m_buffer = {};
            m_capacity = 0;
            m_usedBytes = {};
        }
    }

    void dynamic_buffer::resize(VkCommandBuffer cmd, u32 size)
    {
        reserve(cmd, size);
        m_usedBytes = size;
    }

    void dynamic_buffer::reserve(VkCommandBuffer cmd, u32 size)
    {
        if (size <= m_capacity)
        {
            return;
        }

        const auto oldBuffer = m_buffer;

        allocated_buffer newBuffer{};

        auto& allocator = m_ctx->get_allocator();

        OBLO_VK_PANIC(allocator.create_buffer(
            {
                .size = size,
                .usage = m_usage,
                .requiredFlags = m_memoryPropertyFlags,
            },
            &newBuffer));

        if (m_usedBytes > 0)
        {
            const VkBufferCopy region{.size = m_usedBytes};
            vkCmdCopyBuffer(cmd, oldBuffer.buffer, newBuffer.buffer, 1, &region);
        }

        if (oldBuffer.buffer)
        {
            m_ctx->destroy_deferred(oldBuffer.buffer, m_ctx->get_submit_index());
        }

        if (oldBuffer.allocation)
        {
            m_ctx->destroy_deferred(oldBuffer.allocation, m_ctx->get_submit_index());
        }

        m_buffer = newBuffer;
        m_capacity = size;
    }

    buffer dynamic_buffer::get_buffer() const
    {
        return {.buffer = m_buffer.buffer, .size = m_usedBytes};
    }

    u32 dynamic_buffer::get_capacity() const
    {
        return m_capacity;
    }

    u32 dynamic_buffer::get_used_bytes() const
    {
        return m_usedBytes;
    }
}