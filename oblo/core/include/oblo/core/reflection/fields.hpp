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
}