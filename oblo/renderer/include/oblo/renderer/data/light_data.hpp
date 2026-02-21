#pragma once

#include <oblo/core/handle.hpp>
#include <oblo/core/types.hpp>
#include <oblo/math/vec3.hpp>

namespace oblo
{
    enum class light_type : u32
    {
        point,
        spot,
        directional,
    };

    struct light_data;

    struct light_data
    {
        vec3 position;
        f32 invSqrRadius;
        vec3 direction;
        light_type type;
        vec3 intensity;
        f32 lightAngleScale;
        f32 lightAngleOffset;
        f32 shadowBias;
        f32 _padding[2];
    };

    struct light_config
    {
        u32 lightsCount;
    };
}