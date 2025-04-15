#pragma once

#include <oblo/core/string/string.hpp>
#include <oblo/reflection/codegen/annotations.hpp>

namespace oblo
{
    struct name_component
    {
        string value;
    } OBLO_COMPONENT(ScriptAPI);
}
