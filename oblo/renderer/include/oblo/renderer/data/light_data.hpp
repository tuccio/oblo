#pragma once

#include <oblo/core/handle.hpp>
#include <oblo/core/flags.hpp>
#include <oblo/core/types.hpp>
#include <oblo/math/vec3.hpp>

namespace oblo
{
    // Enums need to match lights.glsl
    enum class gpu_light_type : u32
    {
        point,
        spot,
        directional,
    };

    enum class gpu_light_flags : u8
    {
        shadow_caster,
        hard_shadows,
    };

    struct light_data;

    struct light_data
    {
        vec3 position;
        f32 invSqrRadius;
        vec3 direction;
        gpu_light_type type;
        vec3 intensity;
        f32 lightAngleScale;
        f32 lightAngleOffset;
        f32 shadowBias;
        f32 shadowPunctualRadius;
        flags<gpu_light_flags, 32> flags;
    };

    struct light_config
    {
        u32 lightsCount;
    };
}