#ifndef OBLO_INCLUDE_RENDERER_SPATIAL_HASH
#define OBLO_INCLUDE_RENDERER_SPATIAL_HASH

#include <renderer/random/random>

const uint HASH_MAP_ENTRY_STATE_FREE = 0;
const uint HASH_MAP_ENTRY_STATE_RESERVED = 1;
const uint HASH_MAP_ENTRY_STATE_USED = 2;

struct hash_map_entry
{
    uint state;
    uint id;
};

uint hash_map_calculate(in uint id)
{
    const uint seed = 0xC0FFEE42u;
    return hash_tea(id, seed);
}

uint hash_map_index_probe(in uint h, in uint i, in uint tableMask)
{
    return (h + i) & tableMask;
}

#define HASH_MAP_TRY_ACQUIRE(Entry, Id, Result)                                                                        \
    {                                                                                                                  \
        const uint hashMapEntryPrevState =                                                                             \
            atomicCompSwap((Entry).state, HASH_MAP_ENTRY_STATE_FREE, HASH_MAP_ENTRY_STATE_RESERVED);                   \
                                                                                                                       \
        if (hashMapEntryPrevState == HASH_MAP_ENTRY_STATE_FREE)                                                        \
        {                                                                                                              \
            (Entry).id = Id;                                                                                           \
            atomicExchange((Entry).state, HASH_MAP_ENTRY_STATE_USED);                                                  \
        }                                                                                                              \
        Result = (hashMapEntryPrevState == HASH_MAP_ENTRY_STATE_FREE) ||                                               \
            ((hashMapEntryPrevState == HASH_MAP_ENTRY_STATE_USED) && ((Entry).id == Id));                              \
    }

bool hash_map_try_find(in hash_map_entry e, in uint id, out bool entryUsed)
{
    // If the entry is used but the id is not the same, we want to keep searching
    const bool inUse = e.state == HASH_MAP_ENTRY_STATE_USED;
    entryUsed = inUse;
    return inUse && e.id == id;
}

#endif
