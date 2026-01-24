#pragma once

#include <oblo/core/string/hashed_string_view.hpp>
#include <oblo/core/unique_ptr.hpp>
#include <oblo/reflection/codegen/annotations.hpp>
#include <oblo/resource/resource_ptr.hpp>
#include <oblo/resource/resource_ref.hpp>
#include <oblo/script/interpreter.hpp>

namespace oblo
{
    struct compiled_script;
    struct compiled_bytecode_module;
    struct compiled_native_module;

    struct script_behaviour_component
    {
        resource_ref<compiled_script> script{};
    } OBLO_COMPONENT(ScriptAPI);

    struct script_behaviour_state_component
    {
        using set_global_context_fn = void (*)(void*);
        using execute_fn = void (*)();

        resource_ptr<compiled_script> script{};
        resource_ptr<compiled_bytecode_module> bytecode{};
        resource_ptr<compiled_native_module> native{};
        unique_ptr<interpreter> runtime;
        set_global_context_fn setGlobalContext{};
        execute_fn execute{};
        bool fallbackToInterpreted{};
    } OBLO_COMPONENT(Transient);

    struct script_behaviour_update_tag
    {
    } OBLO_TAG(Transient);
}