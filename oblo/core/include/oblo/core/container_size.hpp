#pragma once

#include <oblo/core/types.hpp>

namespace oblo
{
    template <typename T, u32 N>
    constexpr u32 container_size(const T (&)[N]) noexcept
    {
        return N;
    }

    template <typename T>
    constexpr auto container_size(const T& c) noexcept
        requires(const T& c)
    {
        c.size();
    }
    {
        return c.size();
    }
}