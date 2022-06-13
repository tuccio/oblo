#pragma once

namespace oblo
{
    template <typename T, auto N>
    constexpr auto size(const T (&)[N]) noexcept
    {
        return N;
    }
}