#pragma once

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

    std::span<const component_type> get_components(const archetype_storage& storage);

    u32 get_used_chunks_count(const archetype_storage& storage);

    bool fetch_component_offsets(
        const archetype_storage& storage, std::span<const component_type> componentTypes, std::span<u32> offsets);

    u32 fetch_chunk_data(const archetype_storage& storage,
        u32 chunkIndex,
        std::span<const u32> offsets,
        const entity** entities,
        std::span<std::byte*> componentData);
}