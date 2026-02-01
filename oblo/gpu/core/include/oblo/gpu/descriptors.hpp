#pragma once

#include <oblo/core/flags.hpp>
#include <oblo/gpu/forward.hpp>

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
}