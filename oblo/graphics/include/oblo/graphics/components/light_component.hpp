#pragma once

#include <oblo/core/types.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/vulkan/data/light_data.hpp>

namespace oblo
{
    using light_type = vk::light_type;

    struct light_component
    {
        vec3 color;
        f32 intensity;
        light_type type;
    };
}