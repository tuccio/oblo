#pragma once

#include <oblo/core/debug.hpp>
#include <oblo/core/types.hpp>
#include <oblo/ecs/handles.hpp>

#include <span>

namespace oblo::ecs
{
    struct archetype_impl;

    struct archetype_storage
    {
        archetype_impl* archetype;
    };

    std::span<const component_type> get_component_types(const archetype_storage& storage);

    u32 get_used_chunks_count(const archetype_storage& storage);

    u32 get_entities_count(const archetype_storage& storage);
    u32 get_entities_count_in_chunk(const archetype_storage& storage, u32 chunkIndex);

    bool fetch_component_offsets(
        const archetype_storage& storage, std::span<const component_type> componentTypes, std::span<u32> offsets);

    u32 fetch_chunk_data(const archetype_storage& storage,
        u32 chunkIndex,
        std::span<const u32> offsets,
        const entity** entities,
        std::span<std::byte*> componentData);

    template <typename F>
    void for_each_chunk(const archetype_storage& storage,
        std::span<const component_type> componentTypes,
        std::span<u32> componentOffsets,
        std::span<std::byte*> componentArrays,
        F&& f)
    {
        OBLO_ASSERT(componentTypes.size() == componentOffsets.size());
        OBLO_ASSERT(componentTypes.size() == componentArrays.size());

        const entity* entities;
        fetch_component_offsets(storage, componentTypes, componentOffsets);

        for (u32 chunkIndex = 0, chunkEnd = get_used_chunks_count(storage); chunkIndex != chunkEnd; ++chunkIndex)
        {
            fetch_chunk_data(storage, chunkIndex, componentOffsets, &entities, componentArrays);
            const u32 numEntitiesInChunk = get_entities_count_in_chunk(storage, chunkIndex);
            f(entities, componentArrays, numEntitiesInChunk);
        }
    }
}