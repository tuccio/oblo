#pragma once

#include <oblo/core/hash.hpp>

#include <unordered_map>

namespace oblo
{
    template <class Key,
        typename T,
        typename Hash = hash<Key>,
        typename KeyEqual = std::equal_to<Key>,
        typename Allocator = std::allocator<std::pair<const Key, T>>>
    using unordered_map = std::unordered_map<Key, T, Hash, KeyEqual, Allocator>;
}