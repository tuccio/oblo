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
    } OBLO_COMPONENT("8b56e9f9-ece2-4b08-ba9f-a59256d97080", GpuComponent = "i_TransformBuffer", ScriptAPI);
}