#pragma once

#include <oblo/dotnet/resources/dotnet_assembly.hpp>
#include <oblo/reflection/codegen/annotations.hpp>
#include <oblo/resource/resource_ref.hpp>

namespace oblo
{
    struct dotnet_behaviour_component
    {
        resource_ref<dotnet_assembly> script{};
    } OBLO_COMPONENT();
}