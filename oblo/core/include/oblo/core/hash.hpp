#pragma once

#include <oblo/core/types.hpp>

namespace oblo
{
    constexpr u32 hash_mix(u32 seed, u32 hash)
    {
        seed ^= hash + 0x9e3779b9u + (seed << 6) + (seed >> 2);
        return seed;
    }

    constexpr u64 hash_mix(u64 seed, u64 hash)
    {
        seed ^= hash + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
        return seed;
    }
}