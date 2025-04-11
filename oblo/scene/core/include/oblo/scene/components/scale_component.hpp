#pragma once

#include <oblo/reflection/codegen/annotations.hpp>
#include <oblo/math/vec3.hpp>

namespace oblo
{
    struct scale_component
    {
        vec3 value;
    } OBLO_COMPONENT(ScriptAPI);
}