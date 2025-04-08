#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/reflection/codegen/annotations.hpp>
#include <oblo/resource/resource_traits.hpp>

namespace oblo
{
    struct luau_bytecode
    {
        dynamic_array<byte> byteCode;
    } OBLO_RESOURCE();

    template <>
    struct resource_traits<luau_bytecode>
    {
        static constexpr uuid uuid = "b2455577-8871-488b-b51d-29e29353f9e3"_uuid;
    };
}