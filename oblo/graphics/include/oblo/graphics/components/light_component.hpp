#pragma once

#include <oblo/core/types.hpp>
#include <oblo/math/angle.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/vulkan/data/light_data.hpp>

namespace oblo
{
    using light_type = vk::light_type;

    struct light_component
    {
        light_type type;
        vec3 color;
        f32 intensity;
        f32 radius;
        radians spotInnerAngle;
        radians spotOuterAngle;
        bool isShadowCaster;
        u32 shadowSamples;
        f32 shadowBias;
    };
}