#pragma once

#include <oblo/core/debug.hpp>

#include <utility>

namespace oblo
{
    template <typename T>
    constexpr T min(const T lhs, const T rhs) noexcept
    {
        return lhs < rhs ? lhs : rhs;
    }

    template <typename T, typename... U>
    constexpr T min(const T first, const T second, const T third, U... rest) noexcept
    {
        return min(min(first, second), third, rest...);
    }

    template <typename T>
    constexpr T max(const T lhs, const T rhs) noexcept
    {
        return lhs > rhs ? lhs : rhs;
    }

    template <typename T, typename... U>
    constexpr T max(const T first, const T second, const T third, U... rest) noexcept
    {
        return max(max(first, second), third, rest...);
    }

    template <typename T, typename U>
        requires std::is_arithmetic_v<T>
    constexpr T narrow_cast(U u) noexcept
    {
        OBLO_ASSERT(static_cast<U>(static_cast<T>(u)) == u, "A narrow cast failed");
        return static_cast<T>(u);
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