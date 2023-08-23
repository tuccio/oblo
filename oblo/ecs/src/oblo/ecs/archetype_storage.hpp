#pragma once

#include <oblo/core/array_size.hpp>
#include <oblo/core/types.hpp>
#include <oblo/ecs/component_type_desc.hpp>
#include <oblo/ecs/type_set.hpp>

#include <span>

#define OBLO_ECS_DEBUG_DATA 0

namespace oblo::ecs
{
    class type_registry;
    struct memory_pool_impl;
    struct type_set;

    static constexpr u32 ChunkSize{1u << 14};
    static constexpr u8 InvalidComponentIndex{MaxComponentTypes + 1};
    static constexpr usize PageAlignment{alignof(std::max_align_t)};

    struct chunk
    {
        alignas(PageAlignment) std::byte data[ChunkSize];
    };

    struct component_fn_table
    {
        create_fn create;
        destroy_fn destroy;
        move_fn move;
        move_assign_fn moveAssign;
    };

    struct archetype_storage
    {
        type_set signature;
        component_type* components;
        u32* offsets;
        u32* sizes;
        u32* alignments;
        component_fn_table* fnTables;
        chunk** chunks;
        u32 numEntitiesPerChunk;
        u32 numCurrentChunks;
        u32 numCurrentEntities;
        u8 numComponents;
#if OBLO_ECS_DEBUG_DATA
        type_id* typeIds;
#endif
    };

    static_assert(MaxComponentTypes <= std::numeric_limits<decltype(archetype_storage::numComponents)>::max());

    archetype_storage* create_archetype_storage(memory_pool_impl& pool,
                                                const type_registry& typeRegistry,
                                                const type_set& signature,
                                                std::span<const component_type> components);

    void destroy_archetype_storage(memory_pool_impl& pool, archetype_storage* storage);

    std::span<component_type> make_type_span(std::span<component_type, MaxComponentTypes> inOut, type_set current);

    inline entity* get_entity_pointer(std::byte* chunk, u32 offset)
    {
        return reinterpret_cast<entity*>(chunk) + offset;
    }

    inline std::byte* get_component_pointer(std::byte* chunk,
                                            archetype_storage& archetype,
                                            u8 componentIndex,
                                            u32 offset)
    {
        return chunk + archetype.offsets[componentIndex] + offset * archetype.sizes[componentIndex];
    }

    void reserve_chunks(memory_pool_impl& pool, archetype_storage& archetype, u32 newCount);

    // TODO: Could be implemented with bitwise operations and type_set instead
    inline u8 find_component_index(std::span<const component_type> types, component_type component)
    {
        for (const component_type& c : types)
        {
            if (c == component)
            {
                return u8(&c - types.data());
            }
        }

        return InvalidComponentIndex;
    }

    struct entity_location
    {
        u32 index;
        u32 offset;
    };

    inline entity_location get_entity_location(const archetype_storage& archetype, u32 archetypeIndex)
    {
        const u32 numEntitiesPerChunk = archetype.numEntitiesPerChunk;
        return {archetypeIndex / numEntitiesPerChunk, archetypeIndex % numEntitiesPerChunk};
    }
}