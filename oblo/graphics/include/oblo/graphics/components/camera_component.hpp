#pragma once

#include <oblo/core/types.hpp>
#include <oblo/math/vec3.hpp>

namespace oblo
{
    struct camera_component
    {
        f32 fovy;
        f32 near;
        f32 far;
    };
}