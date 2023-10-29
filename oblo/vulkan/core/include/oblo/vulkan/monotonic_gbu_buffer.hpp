#pragma once

#include <oblo/core/types.hpp>

#include <vector>

#include <vulkan/vulkan.h>

namespace oblo::vk
{
    enum class memory_usage : u8;

    class vulkan_context;
    struct buffer;

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
        std::vector<buffer_info> m_buffers;
        u32 m_currentIndex{};
        u32 m_spaceInCurrentChunk{};
        VkBufferUsageFlags m_usage{};
        u32 m_chunkSize{};
        u8 m_alignment{};
        memory_usage m_memoryUsage{};
    };
}