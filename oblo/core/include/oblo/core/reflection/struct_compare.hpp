#pragma once

#include <oblo/core/struct_apply.hpp>

#include <concepts>

namespace oblo
{
    template <template <typename> typename Compare, typename T>
    concept is_comparable = requires(T v) {
        {
            Compare<T>{}(v, v)
        } -> std::convertible_to<bool>;
    };

    template <typename T>
    struct equal_to;

    template <std::equality_comparable T>
    struct equal_to<T>
    {
        constexpr bool operator()(const T& lhs, const T& rhs) const noexcept
        {
            return lhs == rhs;
        }
    };

    template <template <typename> typename Compare, typename T>
        requires is_comparable<Compare, T>
    constexpr auto struct_compare(const T& lhs, const T& rhs)
    {
        return Compare<T>{}(lhs, rhs);
    }

    template <template <typename> typename Compare, typename T>
        requires(!is_comparable<Compare, T>)
    constexpr auto struct_compare(const T& lhs, const T& rhs)
    {
        return struct_apply(
            [&rhs](auto&&... lm) -> bool {
                return struct_apply([&lm...](auto&&... rm) -> bool { return (struct_compare<Compare>(lm, rm) && ...); },
                    rhs);
            },
            lhs);
    }
}