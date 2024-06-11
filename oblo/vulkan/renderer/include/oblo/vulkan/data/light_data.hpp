#pragma once

#include <oblo/core/types.hpp>
#include <oblo/math/vec3.hpp>

namespace oblo::vk
{
    enum class light_type : u32
    {
        point,
        directional,
    };

    struct light_data
    {
        vec3 position;
        f32 _padding0;
        vec3 direction;
        f32 _padding1;
        vec3 intensity;
        light_type type;
    };

    struct light_config
    {
        u32 lightsCount;
    };
}