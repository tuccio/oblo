#pragma once

#include <oblo/math/mat4.hpp>

namespace oblo
{
    struct global_transform_component
    {
        mat4 localToWorld;
        mat4 normalMatrix;
    };
}