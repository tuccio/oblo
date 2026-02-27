#include <oblo/renderer/draw/monotonic_gbu_buffer.hpp>

#include <oblo/core/utility.hpp>
#include <oblo/gpu/gpu_instance.hpp>
#include <oblo/gpu/structs.hpp>


namespace oblo
{
    struct monotonic_gpu_buffer::buffer_info
    {
        h32<gpu::buffer> handle{};
    };

    monotonic_gpu_buffer::monotonic_gpu_buffer() = default;
    monotonic_gpu_buffer::monotonic_gpu_buffer(monotonic_gpu_buffer&&) noexcept = default;

    monotonic_gpu_buffer& monotonic_gpu_buffer::operator=(monotonic_gpu_buffer&&) noexcept = default;

    monotonic_gpu_buffer::~monotonic_gpu_buffer() = default;

    void monotonic_gpu_buffer::init(
        flags<gpu::buffer_usage> usage, gpu::memory_usage memoryUsage, u8 alignment, u64 chunkSize)
    {
        m_usage = usage;
        m_chunkSize = chunkSize;
        m_currentIndex = ~u32{};
        m_spaceInCurrentChunk = 0;
        m_memoryUsage = memoryUsage;
        m_alignment = alignment;
    }

    void monotonic_gpu_buffer::shutdown(gpu::gpu_instance& ctx)
    {
        const u64 submitIndex = ctx.get_submit_index();

        for (auto& buffer : m_buffers)
        {
            ctx.destroy_deferred(buffer.handle, submitIndex);
        }

        m_buffers.clear();
    }

    expected<gpu::buffer_range> monotonic_gpu_buffer::allocate(gpu::gpu_instance& gpu, u64 size)
    {
        if (size == 0)
        {
            size = m_alignment;
        }

        const auto alignedSize = round_up_multiple<u64>(size, m_alignment);

        OBLO_ASSERT(alignedSize <= m_chunkSize);

        if (alignedSize > m_spaceInCurrentChunk)
        {
            ++m_currentIndex;

            if (m_currentIndex == m_buffers.size())
            {
                const expected newBuffer = gpu.create_buffer({
                    .size = m_chunkSize,
                    .memoryProperties = {m_memoryUsage},
                    .usages = m_usage,
                });

                if (!newBuffer)
                {
                    return "Failed to allocate GPU buffer"_err;
                }

                m_buffers.emplace_back(*newBuffer);
            }

            m_spaceInCurrentChunk = m_chunkSize;
        }

        const auto offset = m_chunkSize - m_spaceInCurrentChunk;
        OBLO_ASSERT(offset % m_alignment == 0);

        m_spaceInCurrentChunk -= alignedSize;

        return gpu::buffer_range{
            .buffer = m_buffers[m_currentIndex].handle,
            .offset = offset,
            .size = size,
        };
    }

    void monotonic_gpu_buffer::restore_all()
    {
        m_currentIndex = ~u32{};
        m_spaceInCurrentChunk = 0;
    }
}