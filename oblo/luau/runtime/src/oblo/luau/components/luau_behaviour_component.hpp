#pragma once

#include <oblo/luau/resources/luau_bytecode.hpp>
#include <oblo/reflection/codegen/annotations.hpp>
#include <oblo/resource/resource_ref.hpp>

namespace oblo
{
    struct luau_behaviour_component
    {
        resource_ref<luau_bytecode> script;
    } OBLO_COMPONENT();

    struct luau_behaviour_loaded_tag
    {
    } OBLO_TAG();
}