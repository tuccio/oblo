#pragma once

#include <oblo/math/mat4.hpp>
#include <oblo/reflection/codegen/annotations.hpp>

namespace oblo
{
    struct global_transform_component
    {
        mat4 localToWorld;
        mat4 lastFrameLocalToWorld;
        mat4 normalMatrix;
    } OBLO_COMPONENT(GpuComponent = "i_TransformBuffer");
}