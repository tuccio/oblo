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
    } OBLO_COMPONENT("64cc6cc8-4a66-4e11-b86e-6cf89080924c", ScriptAPI);

    struct dotnet_behaviour_state_component
    {
        resource_ptr<dotnet_assembly> script{};
        bool initialized{};
    } OBLO_COMPONENT("8e9585d2-b2dc-4bfd-8e0c-3dccf3705db9", Transient);
}