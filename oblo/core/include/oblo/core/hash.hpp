#pragma once

#include <oblo/core/concepts/hashable.hpp>
#include <oblo/core/concepts/sequential_container.hpp>
#include <oblo/core/platform/compiler.hpp>
#include <oblo/core/types.hpp>

#if UINTPTR_MAX == 0xFFFFFFFFFFFFFFFFu
    #include <xxhashct/xxh64.hpp>
    #define OBLO_XXHASHZ xxh64
#elif UINTPTR_MAX == 0xFFFFFFFF
    #include <xxhashct/xxh32.hpp>
    #define OBLO_XXHASHZ xxh32
#endif

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

    usize hash_xxhz(const void* p, u64 size, usize seed = 0);

    OBLO_FORCEINLINE constexpr usize hash_xxhz_compile_time(const char* p, u64 size, usize seed = 0)
    {
        return OBLO_XXHASHZ{}.hash(p, size, seed);
    }

    using hash_type = usize;

    template <typename T>
    struct hash;

    template <typename T>
        requires std::is_integral_v<T> || std::is_enum_v<T>
    struct hash<T>
    {
        hash_type operator()(const T& v) const noexcept
        {
            return hash_xxhz(&v, sizeof(T));
        }
    };

    template <typename T>
        requires sequential_container<T> && hashable<typename T::value_type>
    struct hash<T>
    {
        hash_type operator()(const T& v) const noexcept
        {
            return hash_xxhz(v.data(), sizeof(*v.data()) * v.size());
        }
    };
}