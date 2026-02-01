#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/flags.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/gpu/forward.hpp>
#include <oblo/gpu/types.hpp>

#include <span>

namespace oblo::gpu
{
    template <typename T = success_tag>
    using result = expected<T, error>;

    class gpu_instance
    {
    public:
        virtual ~gpu_instance() = default;

        virtual result<> init() = 0;
        virtual void shutdown() = 0;

        virtual result<h32<surface>> create_surface(const surface_descriptor& surface) = 0;

        virtual result<> create_device_and_queues(const device_descriptor& deviceDesciptors,
            std::span<const queue_descriptor> queueDescriptors,
            std::span<h32<queue>> outQueues) = 0;

        virtual result<h32<swapchain>> create_swapchain(const swapchain_descriptor& swapchain) = 0;

        virtual result<h32<command_buffer_pool>> create_command_buffer_pool(
            const command_buffer_pool_descriptor& init) = 0;

        virtual result<> fetch_command_buffers(h32<command_buffer_pool> pool,
            std::span<hptr<command_buffer>> commandBuffers) = 0;

        virtual result<h32<buffer>> create_buffer(const buffer_descriptor& descriptor) = 0;
        virtual result<h32<image>> create_image(const image_descriptor& descriptor) = 0;

        virtual result<> submit(const queue_submit_descriptor& descriptor) = 0;
    };
}