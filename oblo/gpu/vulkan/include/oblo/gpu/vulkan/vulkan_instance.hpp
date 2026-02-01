#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/handle_flat_pool_map.hpp>
#include <oblo/gpu/gpu_instance.hpp>
#include <oblo/gpu/vulkan/gpu_allocator.hpp>

#include <vulkan/vulkan_core.h>

namespace oblo::gpu
{
    class vulkan_instance final : public gpu_instance
    {
    public:
        vulkan_instance();
        vulkan_instance(const vulkan_instance&) = delete;
        vulkan_instance(vulkan_instance&&) noexcept = delete;
        ~vulkan_instance();

        vulkan_instance& operator=(const vulkan_instance&) = delete;
        vulkan_instance& operator=(vulkan_instance&&) noexcept = delete;

        result<> init(const instance_descriptor& descriptor) override;
        void shutdown() override;

        result<hptr<surface>> create_surface(hptr<native_window> nativeWindow) override;
        void destroy_surface(hptr<surface> surface) override;

        result<> finalize_init(const device_descriptor& deviceDescriptor, hptr<surface> presentSurface) override;

        h32<queue> get_universal_queue() override;

        result<h32<swapchain>> create_swapchain(const swapchain_descriptor& descriptor) override;
        void destroy_swapchain(h32<swapchain> handle) override;

        result<h32<image>> acquire_swapchain_image(h32<swapchain> handle, h32<semaphore> waitSemaphore) override;

        result<h32<command_buffer_pool>> create_command_buffer_pool(
            const command_buffer_pool_descriptor& descriptor) override;

        result<> fetch_command_buffers(h32<command_buffer_pool> pool,
            std::span<hptr<command_buffer>> commandBuffers) override;

        result<h32<buffer>> create_buffer(const buffer_descriptor& descriptor) override;
        result<h32<image>> create_image(const image_descriptor& descriptor) override;

        result<> submit(h32<queue> handle, const queue_submit_descriptor& descriptor) override;

    private:
        struct queue_impl;
        struct swapchain_impl;

    private:
        VkInstance m_instance{};
        VkPhysicalDevice m_physicalDevice{};
        VkDevice m_device{};
        vk::gpu_allocator m_allocator;
        dynamic_array<queue_impl> m_queues;
        h32_flat_pool_dense_map<swapchain, swapchain_impl> m_swapchains;
    };
}