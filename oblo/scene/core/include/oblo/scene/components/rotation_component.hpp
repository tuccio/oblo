#pragma once

#include <oblo/reflection/codegen/annotations.hpp>
#include <oblo/math/quaternion.hpp>

namespace oblo
{
    struct rotation_component
    {
        quaternion value;
    } OBLO_COMPONENT("7ef5fc6a-7b9c-491c-837f-d619747e9b50", ScriptAPI);
}