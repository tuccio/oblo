#pragma once

#include <oblo/core/hash.hpp>

#include <unordered_set>

namespace oblo
{
    template <class Key,
        typename Hash = hash<Key>,
        typename KeyEqual = std::equal_to<Key>,
        typename Allocator = std::allocator<Key>>
    using unordered_set = std::unordered_set<Key, Hash, KeyEqual, Allocator>;
}