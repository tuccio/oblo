#pragma once

#include <oblo/core/types.hpp>

#include <concepts>
#include <type_traits>

namespace oblo
{
    using hash_type = u64;

    template <typename T>
    struct hash;

    template <typename T>
    concept hashable = requires(T v, hash_type h) { h = hash<T>{}(v); };

    template <typename T>
    concept has_hash_value = requires(T v) {
        { hash_value(v) } -> std::same_as<hash_type>;
    };
}