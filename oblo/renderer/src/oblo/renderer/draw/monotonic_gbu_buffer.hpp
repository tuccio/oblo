#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/flags.hpp>
#include <oblo/core/types.hpp>
#include <oblo/gpu/error.hpp>
#include <oblo/gpu/forward.hpp>
#include <oblo/gpu/types.hpp>

namespace oblo
{
    class monotonic_gpu_buffer
    {
    public:
        monotonic_gpu_buffer();
        monotonic_gpu_buffer(const monotonic_gpu_buffer&) = delete;
        monotonic_gpu_buffer(monotonic_gpu_buffer&&) noexcept;

        monotonic_gpu_buffer& operator=(const monotonic_gpu_buffer&) = delete;
        monotonic_gpu_buffer& operator=(monotonic_gpu_buffer&&) noexcept;

        ~monotonic_gpu_buffer();

        void init(flags<gpu::buffer_usage> usage, gpu::memory_usage memoryUsage, u8 alignment, u32 chunkSize);
        void shutdown(gpu::gpu_queue_context& ctx);

        expected<gpu::buffer_range> allocate(gpu::gpu_instance& gpu, u32 size);

        void restore_all();

    private:
        struct buffer_info;

    private:
        dynamic_array<buffer_info> m_buffers;
        u32 m_currentIndex{};
        u32 m_spaceInCurrentChunk{};
        u32 m_chunkSize{};
        flags<gpu::buffer_usage> m_usage{};
        u8 m_alignment{};
        gpu::memory_usage m_memoryUsage{};
    };
}