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

    template <template <typename> typename Hash, typename T, typename... Others>
    constexpr auto hash_all(const T& first, const Others&... args)
    {
        auto h = Hash<T>{}(first);
        ((h = hash_mix(h, Hash<Others>{}(args))), ...);
        return h;
    }

    u32 hash_xxh32(const void* p, u64 size, u32 seed = 0);

    u64 hash_xxh64(const void* p, u64 size, u64 seed = 0);
}