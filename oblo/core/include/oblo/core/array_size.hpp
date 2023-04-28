#pragma once

#include <oblo/core/types.hpp>

namespace oblo
{
    template <typename T, u32 N>
    constexpr u32 array_size(const T (&)[N]) noexcept
    {
        return N;
    }
}