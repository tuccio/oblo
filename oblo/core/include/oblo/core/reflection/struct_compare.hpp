#pragma once

#include <oblo/core/struct_apply.hpp>

namespace oblo
{
    template <template <typename> typename Compare, typename T>
    constexpr bool struct_compare(const T& lhs, const T& rhs)
    {
        if constexpr (requires(T v) { Compare<T>{}(v, v); })
        {
            return Compare<T>{}(lhs, rhs);
        }
        else
        {
            return struct_apply(
                [&rhs](auto&&... lm) -> bool
                {
                    return struct_apply([&rhs, &lm...](auto&&... rm) -> bool
                        { return (struct_compare<Compare>(lm, rm) && ...); },
                        rhs);
                },
                lhs);
        }
    }
}