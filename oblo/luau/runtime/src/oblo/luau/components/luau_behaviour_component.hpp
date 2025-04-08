#pragma once

#include <oblo/luau/resources/luau_bytecode.hpp>
#include <oblo/reflection/codegen/annotations.hpp>
#include <oblo/resource/resource_ptr.hpp>
#include <oblo/resource/resource_ref.hpp>

namespace oblo
{
    struct luau_behaviour_component
    {
        resource_ref<luau_bytecode> script{};
        resource_ptr<luau_bytecode> scriptPtr{};
        bool initialized{};
    } OBLO_COMPONENT();
}