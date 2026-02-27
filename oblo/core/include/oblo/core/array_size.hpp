#pragma once

#include <oblo/core/types.hpp>

namespace oblo
{
    template <typename T, usize N>
    constexpr usize array_size(const T (&)[N]) noexcept
    {
        return N;
    }

    template <typename T, u32 N>
    constexpr u32 array_size32(const T (&)[N]) noexcept
    {
        return N;
    }
}