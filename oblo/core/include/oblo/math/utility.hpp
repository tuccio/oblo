#pragma once

namespace oblo
{
    template <typename T>
    T min(T lhs, T rhs)
    {
        return lhs < rhs ? lhs : rhs;
    }

    template <typename T>
    T max(T lhs, T rhs)
    {
        return lhs > rhs ? lhs : rhs;
    }
}