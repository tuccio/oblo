#include <oblo/core/hash.hpp>

#include <xxhash.h>

namespace oblo
{
    u32 hash_xxh32(const void* p, u64 size, u32 seed)
    {
        return XXH32(p, size, seed);
    }

    u64 hash_xxh64(const void* p, u64 size, u64 seed)
    {
        return XXH64(p, size, seed);
    }
}