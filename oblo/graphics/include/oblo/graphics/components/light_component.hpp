#pragma once

#include <oblo/core/types.hpp>
#include <oblo/math/angle.hpp>
#include <oblo/math/vec3.hpp>

namespace oblo
{
    enum class light_type : u8
    {
        point,
        spot,
        directional,
    };

    struct light_component
    {
        light_type type;
        vec3 color;
        f32 intensity;
        f32 radius;
        radians spotInnerAngle;
        radians spotOuterAngle;
        bool isShadowCaster;
        bool hardShadows;
        u32 shadowSamples;
        f32 shadowBias;
        f32 shadowPunctualRadius;

        // These should rather be global settings
        f32 shadowTemporalAccumulationFactor;
        u32 shadowBlurKernel;
        f32 shadowBlurSigma;
    };
}