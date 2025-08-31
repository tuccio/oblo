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

    struct script_behaviour_component
    {
        resource_ref<compiled_script> script{};
    } OBLO_COMPONENT(ScriptAPI);

    struct script_behaviour_state_component
    {
        resource_ptr<compiled_script> script{};
        unique_ptr<interpreter> runtime;
    } OBLO_COMPONENT(Transient);

    struct script_behaviour_update_tag
    {
    } OBLO_TAG(Transient);
}