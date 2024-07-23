#pragma once

#include <oblo/core/types.hpp>

namespace oblo::vk
{
    enum class light_type : u32;

    struct raytraced_shadow_config
    {
        u32 shadowSamples;
        u32 lightIndex;
        light_type type;
    };
}