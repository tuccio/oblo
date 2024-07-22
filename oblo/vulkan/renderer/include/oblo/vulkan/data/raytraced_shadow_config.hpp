#pragma once

#include <oblo/core/types.hpp>

namespace oblo::vk
{
    struct raytraced_shadow_config
    {
        u32 shadowSamples;
        u32 lightIndex;
    };
}