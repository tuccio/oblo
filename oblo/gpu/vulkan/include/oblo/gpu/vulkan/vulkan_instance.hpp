#pragma once

#include <oblo/gpu/gpu_instance.hpp>

#include <vulkan/vulkan_core.h>

VK_DEFINE_HANDLE(VmaAllocator)

namespace oblo::gpu
{
    class vulkan_instance final : public gpu_instance
    {
    public:
        result<> init(const instance_descriptor& descriptor) override;
        void shutdown() override;

        result<hptr<surface>> create_surface(hptr<native_window> nativeWindow) override;
        void destroy_surface(hptr<surface> surface) override;

        result<> create_device_and_queues(const device_descriptor& deviceDescriptors,
            std::span<const queue_descriptor> queueDescriptors,
            std::span<h32<queue>> outQueues) override;

        result<h32<swapchain>> create_swapchain(const swapchain_descriptor& swapchain) override;

        result<h32<command_buffer_pool>> create_command_buffer_pool(
            const command_buffer_pool_descriptor& descriptor) override;

        result<> fetch_command_buffers(h32<command_buffer_pool> pool,
            std::span<hptr<command_buffer>> commandBuffers) override;

        result<h32<buffer>> create_buffer(const buffer_descriptor& descriptor) override;
        result<h32<image>> create_image(const image_descriptor& descriptor) override;

        result<> submit(const queue_submit_descriptor& descriptor) override;

    private:
        VkInstance m_instance{};
        VkPhysicalDevice m_physicalDevice{};
        VkDevice m_device{};
        VmaAllocator m_allocator{};
    };
}