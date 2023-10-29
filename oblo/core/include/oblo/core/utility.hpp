#pragma once

#include <utility>

namespace oblo
{
    template <typename T>
    constexpr T min(const T lhs, const T rhs) noexcept
    {
        return lhs < rhs ? lhs : rhs;
    }

    template <typename T>
    constexpr T max(const T lhs, const T rhs) noexcept
    {
        return lhs > rhs ? lhs : rhs;
    }

    template <typename T, typename U>
    constexpr T narrow_cast(U&& u) noexcept
    {
        return static_cast<T>(std::forward<U>(u));
    }

    template <typename T>
    constexpr T round_up_div(const T numerator, const T denominator)
    {
        return (numerator + denominator - 1) / denominator;
    }

    template <typename T>
    constexpr T round_up_multiple(const T number, const T multiple)
    {
        return round_up_div(number, multiple) * multiple;
    }
}