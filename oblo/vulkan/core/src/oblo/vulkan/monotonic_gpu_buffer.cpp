#include <oblo/vulkan/monotonic_gbu_buffer.hpp>

#include <oblo/core/utility.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/error.hpp>
#include <oblo/vulkan/gpu_allocator.hpp>
#include <oblo/vulkan/vulkan_context.hpp>

namespace oblo::vk
{
    struct monotonic_gpu_buffer::buffer_info : allocated_buffer
    {
    };

    monotonic_gpu_buffer::monotonic_gpu_buffer() = default;
    monotonic_gpu_buffer::monotonic_gpu_buffer(monotonic_gpu_buffer&&) noexcept = default;

    monotonic_gpu_buffer& monotonic_gpu_buffer::operator=(monotonic_gpu_buffer&&) noexcept = default;

    monotonic_gpu_buffer::~monotonic_gpu_buffer() = default;

    void monotonic_gpu_buffer::init(VkBufferUsageFlags usage, memory_usage memoryUsage, u8 alignment, u32 chunkSize)
    {
        m_usage = usage;
        m_chunkSize = chunkSize;
        m_currentIndex = ~u32{};
        m_spaceInCurrentChunk = 0;
        m_memoryUsage = memoryUsage;
        m_alignment = alignment;
    }

    void monotonic_gpu_buffer::shutdown(vulkan_context& ctx)
    {
        const u64 submitIndex = ctx.get_submit_index();

        for (auto& buffer : m_buffers)
        {
            ctx.destroy_deferred(buffer.buffer, submitIndex);
            ctx.destroy_deferred(buffer.allocation, submitIndex);
        }
    }

    buffer monotonic_gpu_buffer::allocate(vulkan_context& ctx, u32 size)
    {
        const auto alignedSize = round_up_multiple<u32>(size, m_alignment);

        OBLO_ASSERT(alignedSize <= m_chunkSize);

        if (alignedSize > m_spaceInCurrentChunk)
        {
            ++m_currentIndex;

            if (m_currentIndex == m_buffers.size())
            {
                auto& allocator = ctx.get_allocator();
                allocated_buffer newBuffer;

                OBLO_VK_PANIC(allocator.create_buffer(
                    {
                        .size = m_chunkSize,
                        .usage = m_usage,
                        .memoryUsage = m_memoryUsage,
                    },
                    &newBuffer));

                m_buffers.emplace_back(newBuffer);
            }

            m_spaceInCurrentChunk = m_chunkSize;
        }

        buffer res;

        const auto offset = m_chunkSize - m_spaceInCurrentChunk;
        OBLO_ASSERT(offset % m_alignment == 0);

        m_spaceInCurrentChunk -= alignedSize;

        res = {
            .buffer = m_buffers[m_currentIndex].buffer,
            .offset = offset,
            .size = size,
        };

        return res;
    }

    void monotonic_gpu_buffer::restore_all()
    {
        m_currentIndex = ~u32{};
        m_spaceInCurrentChunk = 0;
    }
}