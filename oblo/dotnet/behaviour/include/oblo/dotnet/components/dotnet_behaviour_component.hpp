#pragma once

#include <oblo/dotnet/resources/dotnet_assembly.hpp>
#include <oblo/reflection/codegen/annotations.hpp>
#include <oblo/resource/resource_ptr.hpp>
#include <oblo/resource/resource_ref.hpp>

namespace oblo
{
    struct dotnet_behaviour_component
    {
        resource_ref<dotnet_assembly> script{};
    } OBLO_COMPONENT(ScriptAPI);

    struct dotnet_behaviour_state_component
    {
        resource_ptr<dotnet_assembly> script{};
        bool initialized{};
    } OBLO_COMPONENT(Transient);
}