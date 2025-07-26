#pragma once

#include <oblo/reflection/codegen/annotations.hpp>
#include <oblo/script/bytecode_module.hpp>

namespace oblo
{
    class cstring_view;

    struct compiled_script
    {
        bytecode_module module;
    } OBLO_RESOURCE();

    SCRIPT_RESOURCE_API bool save(const compiled_script& script, cstring_view destination);
    SCRIPT_RESOURCE_API bool load(compiled_script& script, cstring_view source);
}