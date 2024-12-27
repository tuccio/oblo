#pragma once

#include <oblo/core/array_size.hpp>
#include <oblo/core/types.hpp>
#include <oblo/ecs/component_type_desc.hpp>
#include <oblo/ecs/type_set.hpp>

#include <span>

namespace oblo
{
    class memory_pool;
}

namespace oblo::ecs
{
    class type_registry;
    struct type_set;

    static constexpr usize PageAlignment{alignof(std::max_align_t)};
    static constexpr u32 ChunkWithHeaderSize{1u << 14};
    static constexpr u32 ChunkSize{ChunkWithHeaderSize - PageAlignment};
    static constexpr u8 InvalidComponentIndex{MaxComponentTypes + 1};

    struct chunk_header
    {
        u32 numEntities;
    };

    struct chunk
    {
        chunk_header header;
        alignas(PageAlignment) std::byte data[ChunkSize];
    };

    struct component_fn_table
    {
        create_fn create;
        destroy_fn destroy;
        move_fn move;
        move_assign_fn moveAssign;

        void do_create(void* dst, usize count)
        {
            if (create)
            {
                create(dst, count);
            }
        }

        void do_destroy(void* dst, usize count)
        {
            if (destroy)
            {
                destroy(dst, count);
            }
        }

        void do_move(usize size, void* dst, void* src, usize count)
        {
            if (move)
            {
                move(dst, src, count);
            }
            else
            {
                std::memcpy(dst, src, size * count);
            }
        }

        void do_move_assign(usize size, void* dst, void* src, usize count)
        {
            if (move)
            {
                moveAssign(dst, src, count);
            }
            else
            {
                std::memcpy(dst, src, size * count);
            }
        }
    };

    struct archetype_impl
    {
        component_and_tag_sets types;
        component_type* components;
        tag_type* tags;
        u32* offsets;
        u32* sizes;
        u32* alignments;
        component_fn_table* fnTables;
        chunk** chunks;
        u32 numEntitiesPerChunk;
        u32 numCurrentChunks;
        u32 numCurrentEntities;
        u32 entityTagsOffset;
        u8 numComponents;
        u8 numTags;
#if OBLO_DEBUG
        type_id* typeIds;
#endif
    };

    struct entity_tags
    {
        type_set types;
    };

    archetype_impl* create_archetype_impl(
        memory_pool& pool, const type_registry& typeRegistry, const component_and_tag_sets& types);

    void destroy_archetype_impl(memory_pool& pool, archetype_impl* storage);

    inline entity* get_entity_pointer(std::byte* chunk, u32 offset)
    {
        return reinterpret_cast<entity*>(chunk) + offset;
    }

    inline entity_tags* get_entity_tags_pointer(std::byte* chunk, const archetype_impl& archetype, u32 offset)
    {
        return reinterpret_cast<entity_tags*>(chunk + archetype.entityTagsOffset + offset);
    }

    inline std::byte* get_component_pointer(
        std::byte* chunk, const archetype_impl& archetype, u8 componentIndex, u32 offset)
    {
        return chunk + archetype.offsets[componentIndex] + offset * archetype.sizes[componentIndex];
    }

    void reserve_chunks(memory_pool& pool, archetype_impl& archetype, u32 newCount);

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

    inline entity_location get_entity_location(const archetype_impl& archetype, u32 archetypeIndex)
    {
        const u32 numEntitiesPerChunk = archetype.numEntitiesPerChunk;
        return {archetypeIndex / numEntitiesPerChunk, archetypeIndex % numEntitiesPerChunk};
    }
}