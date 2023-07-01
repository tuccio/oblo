#pragma once

namespace
{
    template <typename Tag, typename T, T Init = T{}>
    struct handle
    {
        constexpr explicit operator bool() const noexcept
        {
            return value == Init;
        }

        T value{Init};
    };
}