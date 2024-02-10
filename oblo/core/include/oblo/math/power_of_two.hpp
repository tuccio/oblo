#pragma once

namespace oblo
{
    template <typename T>
    constexpr bool is_power_of_two(T x)
    {
        return (x != 0) && ((x & (x - 1)) == 0);
    }

#pragma warning(push)
#pragma warning(disable : 4146)

    template <typename T>
    constexpr T align_power_of_two(T unaligned, T alignment)
    {
        const auto mask = alignment - 1;
        return unaligned + (-unaligned & mask);
    }

#pragma warning(pop)
}