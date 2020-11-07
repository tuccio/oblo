#pragma once

#include <oblo/math/angle.hpp>
#include <oblo/math/vec3.hpp>

namespace oblo
{
    struct camera
    {
        vec3 position;
        vec3 up;
        vec3 direction;
        radians fovx;
        radians fovy;
        float near;
        float far;
    };
}