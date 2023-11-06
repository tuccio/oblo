#pragma once

#include <oblo/core/types.hpp>
#include <oblo/math/angle.hpp>
#include <oblo/math/vec3.hpp>

namespace oblo
{
    struct camera_component
    {
        radians fovy;
        f32 near;
        f32 far;
    };
}