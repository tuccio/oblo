#pragma once

#include <oblo/resource/resource_traits.hpp>

namespace oblo
{
    struct compiled_script;
    struct compiled_bytecode_module;
    struct compiled_native_module;

    template <>
    struct resource_traits<compiled_script>
    {
        static constexpr uuid uuid = "cf278272-f55d-4adf-aa62-6f004c3518cc"_uuid;
    };
    template <>
    struct resource_traits<compiled_bytecode_module>
    {
        static constexpr uuid uuid = "e0274f24-fc5a-41e9-bc25-284a6fe613db"_uuid;
    };

    template <>
    struct resource_traits<compiled_native_module>
    {
        static constexpr uuid uuid = "c7676e73-42f7-4f43-91ee-c4a9ab3d9e0c"_uuid;
    };
}