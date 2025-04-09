#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/reflection/codegen/annotations.hpp>
#include <oblo/resource/resource_traits.hpp>

namespace oblo
{
    struct dotnet_assembly
    {
        dynamic_array<byte> assembly;
    } OBLO_RESOURCE();

    template <>
    struct resource_traits<dotnet_assembly>
    {
        static constexpr uuid uuid = "6ba3c31f-3373-4c27-b7b2-f255797c38e3"_uuid;
    };
}