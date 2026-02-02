#pragma once

#include <oblo/gpu/forward.hpp>
#include <oblo/gpu/types.hpp>

namespace oblo::gpu
{
    struct command_buffer_pool_descriptor
    {
        h32<queue> queue;
        u32 numCommandBuffers;
    };

    struct device_descriptor
    {
        bool requireHardwareRaytracing;
    };

    struct fence_descriptor
    {
        bool createSignaled;
    };

    struct instance_descriptor
    {
        const char* application;
        const char* engine;
    };

    struct present_descriptor
    {
        std::span<const h32<swapchain>> swapchains;
        std::span<const h32<semaphore>> waitSemaphores;
    };

    struct queue_submit_descriptor
    {
        std::span<const hptr<command_buffer>> commandBuffers;
        std::span<const h32<semaphore>> waitSemaphores;
        h32<fence> signalFence;
        std::span<const h32<semaphore>> signalSemaphores;
    };

    struct semaphore_descriptor
    {
    };

    struct swapchain_descriptor
    {
        hptr<surface> surface;
        u32 numImages;
        texture_format format;
        u32 width;
        u32 height;
    };
}