#pragma once

#include <oblo/core/debug.hpp>
#include <oblo/core/platform/compiler.hpp>
#include <oblo/core/preprocessor.hpp>

#include <cmath>
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
    OBLO_INTRINSIC constexpr T narrow_cast(U u) noexcept
    {
        const auto t = static_cast<T>(u);

        if constexpr (std::is_floating_point_v<U> || std::is_floating_point_v<T>)
        {
            // The NaN check would work with the other assert, but we dont want to check equality comparisons on
            // floating points otherwise

            if constexpr (std::is_floating_point_v<U>)
            {
                OBLO_ASSERT(!std::isnan(u), "A narrow cast failed");
            }

            if constexpr (std::is_floating_point_v<T>)
            {
                OBLO_ASSERT(!std::isnan(t), "A narrow cast failed");
            }
        }
        else
        {
            OBLO_ASSERT(static_cast<U>(t) == u, "A narrow cast failed");
        }

        if constexpr (std::is_signed_v<T> != std::is_signed_v<U>)
        {
            OBLO_ASSERT((t >= T{}) && (u >= U{}), "A narrow cast failed");
        }

        return t;
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
