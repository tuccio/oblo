#pragma once

#include <oblo/gpu/forward.hpp>
#include <oblo/gpu/types.hpp>

namespace oblo::gpu
{
    struct device_descriptor
    {
        bool requireHardwareRaytracing;
    };

    struct instance_descriptor
    {
        const char* application;
        const char* engine;
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