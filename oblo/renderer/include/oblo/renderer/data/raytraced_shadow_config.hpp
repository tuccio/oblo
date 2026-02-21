#pragma once

#include <oblo/core/types.hpp>

namespace oblo
{
    enum class light_type : u32;

    struct raytraced_shadow_config
    {
        u32 lightIndex;
        light_type type;
        f32 shadowPunctualRadius;
        f32 temporalAccumulationFactor;
        f32 depthSigma;
        bool hardShadows;
    };
}