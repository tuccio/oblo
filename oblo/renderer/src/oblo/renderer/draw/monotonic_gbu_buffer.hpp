#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/types.hpp>
#include <oblo/vulkan/gpu_temporary_aliases.hpp>

#include <vulkan/vulkan_core.h>

namespace oblo::vk
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

        void init(VkBufferUsageFlags usage, memory_usage memoryUsage, u8 alignment, u32 chunkSize);
        void shutdown(vulkan_context& ctx);

        buffer allocate(vulkan_context& ctx, u32 size);

        void restore_all();

    private:
        struct buffer_info;

    private:
        dynamic_array<buffer_info> m_buffers;
        u32 m_currentIndex{};
        u32 m_spaceInCurrentChunk{};
        VkBufferUsageFlags m_usage{};
        u32 m_chunkSize{};
        u8 m_alignment{};
        memory_usage m_memoryUsage{};
    };
}