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

        virtual device_info get_device_info() = 0;

        virtual h32<queue> get_universal_queue() = 0;

        virtual result<h32<swapchain>> create_swapchain(const swapchain_descriptor& descriptor) = 0;
        virtual void destroy_swapchain(h32<swapchain> handle) = 0;

        virtual result<h32<fence>> create_fence(const fence_descriptor& descriptor) = 0;
        virtual void destroy_fence(h32<fence> handle) = 0;

        virtual result<> wait_for_fences(const std::span<const h32<fence>> fences) = 0;
        virtual result<> reset_fences(const std::span<const h32<fence>> fences) = 0;

        virtual result<h32<semaphore>> create_semaphore(const semaphore_descriptor& descriptor) = 0;
        virtual void destroy_semaphore(h32<semaphore> handle) = 0;
        virtual result<u64> read_timeline_semaphore(h32<semaphore> handle) = 0;

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
        virtual void destroy_buffer(h32<buffer> bufferHandle) = 0;

        virtual h64<device_address> get_device_address(h32<buffer> bufferHandle) = 0;

        virtual result<h32<image>> create_image(const image_descriptor& descriptor) = 0;
        virtual void destroy_image(h32<image> imageHandle) = 0;

        virtual image_descriptor get_image_descriptor(h32<image> imageHandle) = 0;

        virtual result<h32<image_pool>> create_image_pool(std::span<const image_descriptor> descriptors,
            std::span<h32<image>> images) = 0;

        virtual void destroy_image_pool(h32<image_pool> imagePoolHandle) = 0;

        virtual result<h32<shader_module>> create_shader_module(const shader_module_descriptor& descriptor) = 0;
        virtual void destroy_shader_module(h32<shader_module> handle) = 0;

        virtual result<h32<render_pipeline>> create_render_pipeline(const render_pipeline_descriptor& descriptor) = 0;
        virtual void destroy_render_pipeline(h32<render_pipeline> handle) = 0;

        virtual result<h32<sampler>> create_sampler(const sampler_descriptor& descriptor) = 0;
        virtual void destroy_sampler(h32<sampler> handle) = 0;

        virtual result<> begin_render_pass(hptr<command_buffer> cmdBuffer, h32<render_pipeline> pipeline) = 0;
        virtual void end_render_pass(hptr<command_buffer> cmdBuffer) = 0;

        virtual result<h32<bindless_image>> acquire_bindless(h32<image> optImage) = 0;
        virtual result<h32<bindless_image>> replace_bindless(h32<bindless_image> slot, h32<image> optImage) = 0;
        virtual void release_bindless(h32<bindless_image> slot) = 0;

        virtual result<> submit(h32<queue> handle, const queue_submit_descriptor& descriptor) = 0;

        virtual result<> present(const present_descriptor& descriptor) = 0;

        virtual result<> wait_idle() = 0;

        // Memory mapping

        virtual result<void*> memory_map(h32<buffer> buffer) = 0;

        virtual result<> memory_unmap(h32<buffer> buffer) = 0;

        virtual result<> memory_invalidate(std::span<const h32<buffer>> buffers) = 0;

        // Transfer commands

        virtual void cmd_copy_buffer(hptr<command_buffer> cmd,
            h32<buffer> src,
            h32<buffer> dst,
            std::span<const buffer_copy_descriptor> copies) = 0;

        virtual void cmd_copy_buffer_to_image(hptr<command_buffer> cmd,
            h32<buffer> src,
            h32<image> dst,
            std::span<const buffer_image_copy_descriptor> copies) = 0;
    };

    /// @brief To be used to offset device address, to keep track where we make the assumptions on how to offset GPU
    /// addresses.
    inline h64<device_address> offset_device_address(h64<device_address> address, u64 offset)
    {
        return {address.value + offset};
    }
}