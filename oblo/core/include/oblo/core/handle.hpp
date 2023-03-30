#pragma once

namespace
{
    template <typename T, T Init = T{}>
    struct handle
    {
        explicit constexpr operator bool() const noexcept
        {
            return value == Init;
        }

        T value{Init};
    };
}