#pragma once

namespace oblo
{
    template <typename T, auto N>
    constexpr auto array_size(const T (&)[N]) noexcept
    {
        return N;
    }
}