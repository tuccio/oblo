#pragma once

#include <oblo/core/platform/shared_library.hpp>
#include <oblo/reflection/codegen/annotations.hpp>
#include <oblo/resource/resource_ref.hpp>
#include <oblo/resource/resource_traits.hpp>
#include <oblo/script/bytecode_module.hpp>
#include <oblo/script/resources/traits.hpp>

namespace oblo
{
    class cstring_view;

    struct compiled_bytecode_module
    {
        bytecode_module module;
    } OBLO_RESOURCE();

    struct compiled_native_module
    {
        platform::shared_library module;
    } OBLO_RESOURCE();

    struct compiled_script
    {
        resource_ref<compiled_bytecode_module> bytecode;
        resource_ref<compiled_native_module> x86_64_avx2;
    } OBLO_RESOURCE();

    OBLO_SCRIPT_RESOURCE_API bool save(const compiled_script& script, cstring_view destination);
    OBLO_SCRIPT_RESOURCE_API bool load(compiled_script& script, cstring_view source);

    OBLO_SCRIPT_RESOURCE_API bool save(const bytecode_module& script, cstring_view destination);
    OBLO_SCRIPT_RESOURCE_API bool save(const compiled_bytecode_module& script, cstring_view destination);
    OBLO_SCRIPT_RESOURCE_API bool load(compiled_bytecode_module& script, cstring_view source);

    OBLO_SCRIPT_RESOURCE_API bool load(compiled_native_module& script, cstring_view source);
}