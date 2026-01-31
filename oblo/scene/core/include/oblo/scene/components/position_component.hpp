#pragma once

#include <oblo/math/vec3.hpp>
#include <oblo/reflection/codegen/annotations.hpp>

namespace oblo
{
    struct position_component
    {
        vec3 value;
    } OBLO_COMPONENT("06d70f31-13c7-4c19-a1ca-19af48c5eb37", ScriptAPI);
}