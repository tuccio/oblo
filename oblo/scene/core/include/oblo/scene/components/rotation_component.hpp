#pragma once

#include <oblo/reflection/codegen/annotations.hpp>
#include <oblo/math/quaternion.hpp>

namespace oblo
{
    struct rotation_component
    {
        quaternion value;
    } OBLO_COMPONENT();
}