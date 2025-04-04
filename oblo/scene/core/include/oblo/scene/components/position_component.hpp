#pragma once

#include <oblo/core/reflection/annotations.hpp>
#include <oblo/math/vec3.hpp>

namespace oblo
{
    struct position_component
    {
        vec3 value;
    } OBLO_COMPONENT();
}