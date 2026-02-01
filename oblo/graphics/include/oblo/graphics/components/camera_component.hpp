#pragma once

#include <oblo/core/types.hpp>
#include <oblo/math/angle.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/reflection/codegen/annotations.hpp>

namespace oblo
{
    struct camera_component
    {
        radians fovy;
        f32 near;
        f32 far;
    } OBLO_COMPONENT("503720b5-e339-46d3-a29a-1883e9f00bca", ScriptAPI);
}