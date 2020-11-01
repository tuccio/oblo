#pragma once

namespace oblo
{
    template <typename T>
    constexpr T min(const T& lhs, const T& rhs) noexcept
    {
        return lhs < rhs ? lhs : rhs;
    }

    template <typename T>
    constexpr T max(const T& lhs, const T& rhs) noexcept
    {
        return lhs > rhs ? lhs : rhs;
    }

    template <typename T, typename U>
    constexpr T narrow_cast(U&& u) noexcept
    {
        return static_cast<T>(std::forward<U>(u));
    }
}