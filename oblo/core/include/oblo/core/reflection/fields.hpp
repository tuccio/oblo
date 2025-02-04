#pragma once

#include <oblo/core/struct_apply.hpp>
#include <oblo/core/types.hpp>

namespace oblo
{
    template <typename T>
    consteval usize count_fields()
    {
        return struct_apply([]([[maybe_unused]] auto&&... m) { return sizeof...(m); }, T{});
    }

    template <typename T>
    consteval bool struct_has_padding()
    {
        constexpr usize fieldsSize = struct_apply([]<typename... U>(const U&...) { return (sizeof(U) + ...); }, T{});

        return fieldsSize != sizeof(T);
    }
}