#pragma once

#include <oblo/core/handle.hpp>
#include <oblo/gpu/error.hpp>
#include <oblo/gpu/forward.hpp>
#include <oblo/gpu/types.hpp>

#include <span>

namespace oblo::gpu
{
    class gpu_instance
    {
    public:
        virtual ~gpu_instance() = default;

        virtual result<> init(const instance_descriptor& descriptor) = 0;
        virtual void shutdown() = 0;

        virtual result<hptr<surface>> create_surface(hptr<native_window> nativeWindow) = 0;
        virtual void destroy_surface(hptr<surface> surface) = 0;

        virtual result<> finalize_init(const device_descriptor& deviceDescriptor, hptr<surface> presentSurface) = 0;

        virtual h32<queue> get_universal_queue() = 0;

        virtual result<h32<swapchain>> create_swapchain(const swapchain_descriptor& descriptor) = 0;
        virtual void destroy_swapchain(h32<swapchain> handle) = 0;

        virtual result<h32<fence>> create_fence(const fence_descriptor& descriptor) = 0;
        virtual void destroy_fence(h32<fence> handle) = 0;

        virtual result<> wait_for_fences(const std::span<const h32<fence>> fences) = 0;
        virtual result<> reset_fences(const std::span<const h32<fence>> fences) = 0;

        virtual result<h32<semaphore>> create_semaphore(const semaphore_descriptor& descriptor) = 0;
        virtual void destroy_semaphore(h32<semaphore> handle) = 0;

        virtual result<h32<image>> acquire_swapchain_image(h32<swapchain> handle, h32<semaphore> waitSemaphore) = 0;

        virtual result<h32<command_buffer_pool>> create_command_buffer_pool(
            const command_buffer_pool_descriptor& descriptor) = 0;

        virtual result<> fetch_command_buffers(h32<command_buffer_pool> pool,
            std::span<hptr<command_buffer>> commandBuffers) = 0;

        virtual void destroy_command_buffer_pool(h32<command_buffer_pool> commandBufferPool) = 0;

        virtual result<> reset_command_buffer_pool(h32<command_buffer_pool> commandBufferPool) = 0;

        virtual result<> begin_command_buffer(hptr<command_buffer> commandBuffer) = 0;
        virtual result<> end_command_buffer(hptr<command_buffer> commandBuffer) = 0;

        virtual result<h32<buffer>> create_buffer(const buffer_descriptor& descriptor) = 0;
        virtual result<h32<image>> create_image(const image_descriptor& descriptor) = 0;

        virtual result<> submit(h32<queue> handle, const queue_submit_descriptor& descriptor) = 0;

        virtual result<> present(const present_descriptor& descriptor) = 0;

        virtual result<> wait_idle() = 0;
    };
}