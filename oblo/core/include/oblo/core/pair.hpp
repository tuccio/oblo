#pragma once

#include <oblo/core/hash.hpp>

#include <compare>

namespace oblo
{
    template <typename T, typename U>
    struct pair
    {
        T first;
        U second;

        constexpr auto operator<=>(const pair&) const = default;
    };

    template <typename T, typename U>
    struct hash<pair<T, U>>
    {
        auto operator()(const pair<T, U>& p) const noexcept
        {
            return hash_mix(hash<T>{}(p.first), hash<U>{}(p.second));
        }
    };
}