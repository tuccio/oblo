#pragma once

#include <oblo/math/vec3.hpp>

namespace oblo
{
    struct gpu_aabb
    {
        vec3 min;
        f32 _padding0;
        vec3 max;
        f32 _padding1;
    };
}