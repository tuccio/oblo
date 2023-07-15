#pragma once

#include <vulkan/vulkan.h>

#include <oblo/core/ring_buffer.hpp>
#include <oblo/core/types.hpp>

#include <span>

namespace oblo::vk
{
    class command_buffer_pool
    {
    public:
        command_buffer_pool() = default;
        command_buffer_pool(const command_buffer_pool&) = delete;
        command_buffer_pool(command_buffer_pool&&) noexcept;
        command_buffer_pool& operator=(const command_buffer_pool&) = delete;
        command_buffer_pool& operator=(command_buffer_pool&&) noexcept;
        ~command_buffer_pool();

        bool init(
            VkDevice device, u32 queueFamilyIndex, bool resetCommandBuffers, u32 buffersPerFrame, u32 framesInFlight);

        void shutdown();

        void begin_frame(u64 frameIndex);

        void reset_buffers(u64 frameIndex);
        void reset_pool();

        VkCommandBuffer fetch_buffer();
        void fetch_buffers(std::span<VkCommandBuffer> outBuffers);

    private:
        void allocate_buffers(std::size_t count);

    private:
        struct tracking_info
        {
            u64 frameIndex;
            std::size_t count;
        };

        VkDevice m_device{nullptr};
        VkCommandPool m_commandPool{nullptr};
        ring_buffer<VkCommandBuffer> m_commandBuffers;
        ring_buffer<tracking_info> m_trackingInfo;
        bool m_resetCommandBuffers{false};
    };
}