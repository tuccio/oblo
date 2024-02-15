#pragma once

#include <bit>
#include <concepts>

#include <oblo/core/types.hpp>

namespace oblo
{
    template <std::unsigned_integral T>
    constexpr bool is_power_of_two(T x)
    {
        return (x != 0) && ((x & (x - 1)) == 0);
    }

#pragma warning(push)
#pragma warning(disable : 4146)

    template <std::unsigned_integral T>
    constexpr T align_power_of_two(T unaligned, T alignment)
    {
        const auto mask = alignment - 1;
        return unaligned + (-unaligned & mask);
    }

#pragma warning(pop)

    template <std::unsigned_integral T>
    constexpr T round_up_power_of_two(T x)
    {
        const auto log2 = std::countl_zero(x - 1);
        return T(1) << (8 * sizeof(T) - log2);
    }
}