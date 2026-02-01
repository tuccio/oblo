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

    struct swapchain_descriptor
    {
        image_format format;
        u32 numImages;
    };
}