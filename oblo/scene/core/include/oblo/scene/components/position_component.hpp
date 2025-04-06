#pragma once

#include <oblo/math/vec3.hpp>
#include <oblo/reflection/codegen/annotations.hpp>

namespace oblo
{
    struct position_component
    {
        vec3 value;
    } OBLO_COMPONENT();
}