#include <oblo/ecs/archetype_storage.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/ecs/memory_pool_impl.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/math/power_of_two.hpp>

#include <algorithm>

namespace oblo::ecs
{
    static_assert(sizeof(chunk) == ChunkWithHeaderSize);
    static_assert(MaxComponentTypes <= std::numeric_limits<decltype(archetype_storage::numComponents)>::max());

    archetype_storage* create_archetype_storage(memory_pool_impl& pool,
                                                const type_registry& typeRegistry,
                                                const component_and_tags_sets& types,
                                                std::span<const component_type> components)
    {
        OBLO_ASSERT(std::is_sorted(components.begin(), components.end()));
        const u8 numComponents = u8(components.size());

        archetype_storage* storage = new (pool.allocate<archetype_storage>()) archetype_storage{
            .types = types,
            .numComponents = numComponents,
        };

        pool.create_array_uninitialized(storage->components, numComponents);
        pool.create_array_uninitialized(storage->offsets, numComponents);
        pool.create_array_uninitialized(storage->sizes, numComponents);
        pool.create_array_uninitialized(storage->alignments, numComponents);
        pool.create_array_uninitialized(storage->fnTables, numComponents);

#ifdef OBLO_DEBUG
        pool.create_array_uninitialized(storage->typeIds, numComponents);
#endif

        std::memcpy(storage->components, components.data(), components.size_bytes());

        usize columnsSizeSum = sizeof(entity) + sizeof(entity_tags);

        static_assert(alignof(entity_tags) > alignof(entity));
        usize paddingWorstCase = alignof(entity_tags) - alignof(entity);

        {
            usize previousAlignment = alignof(entity_tags);

            for (u8 componentIndex = 0; componentIndex < numComponents; ++componentIndex)
            {
                const auto& typeDesc = typeRegistry.get_component_type_desc(components[componentIndex]);
                OBLO_ASSERT(is_power_of_two(typeDesc.alignment));

                const u32 componentSize = typeDesc.size;

                storage->fnTables[componentIndex] = {
                    .create = typeDesc.create,
                    .destroy = typeDesc.destroy,
                    .move = typeDesc.move,
                    .moveAssign = typeDesc.moveAssign,
                };

                storage->alignments[componentIndex] = typeDesc.alignment;
                storage->sizes[componentIndex] = componentSize;

#ifdef OBLO_DEBUG
                storage->typeIds[componentIndex] = typeDesc.type;
#endif

                columnsSizeSum += componentSize;

                if (previousAlignment < typeDesc.alignment)
                {
                    paddingWorstCase += typeDesc.alignment - previousAlignment;
                }
            }
        }

        const u32 numEntitiesPerChunk = (ChunkSize - paddingWorstCase) / columnsSizeSum;
        storage->numEntitiesPerChunk = numEntitiesPerChunk;

        u32 currentOffset = 0;

        // The initial page alignment
        usize previousAlignment = PageAlignment;

        const auto computePadding = [&previousAlignment](usize newAlignment)
        { return previousAlignment >= newAlignment ? usize(0) : newAlignment - previousAlignment; };

        // First we have entity ids
        currentOffset += computePadding(alignof(entity)) + sizeof(entity) * numEntitiesPerChunk;
        previousAlignment = alignof(entity);

        // Then we have the tags
        storage->entityTagsOffset = currentOffset;

        currentOffset += computePadding(alignof(entity_tags)) + sizeof(entity_tags) * numEntitiesPerChunk;
        previousAlignment = alignof(entity_tags);

        for (u8 componentIndex = 0; componentIndex < numComponents; ++componentIndex)
        {
            const auto size = storage->sizes[componentIndex];
            const auto alignment = storage->alignments[componentIndex];
            const auto padding = computePadding(alignment);

            const u32 startOffset = currentOffset + padding;
            storage->offsets[componentIndex] = startOffset;

            currentOffset = startOffset + size * numEntitiesPerChunk;
            OBLO_ASSERT(currentOffset <= ChunkSize);

            previousAlignment = alignment;
        }

        return storage;
    }

    void destroy_archetype_storage(memory_pool_impl& pool, archetype_storage* storage)
    {
        const auto numComponents = storage->numComponents;

        if (const auto numChunks = storage->numCurrentChunks; numChunks != 0)
        {
            u32 numEntities = storage->numCurrentEntities;
            const u32 numEntitiesPerChunk = storage->numEntitiesPerChunk;

            for (chunk **it = storage->chunks, **end = storage->chunks + numChunks; it != end; ++it)
            {
                if (numEntities != 0)
                {
                    const u32 numEntitiesInChunk = min(numEntities, numEntitiesPerChunk);

                    std::byte* const data = (*it)->data;

                    for (u8 componentIndex = 0; componentIndex < numComponents; ++componentIndex)
                    {
                        std::byte* const componentData = data + storage->offsets[componentIndex];
                        storage->fnTables[componentIndex].destroy(componentData, numEntitiesInChunk);
                    }

                    numEntities -= numEntitiesInChunk;
                }

                pool.deallocate(*it);
            }

            pool.deallocate_array(storage->chunks, numChunks);
        }

        if (numComponents != 0)
        {
            pool.deallocate_array(storage->components, numComponents);
            pool.deallocate_array(storage->offsets, numComponents);
            pool.deallocate_array(storage->sizes, numComponents);
            pool.deallocate_array(storage->alignments, numComponents);
            pool.deallocate_array(storage->fnTables, numComponents);

#ifdef OBLO_DEBUG
            pool.deallocate_array(storage->typeIds, numComponents);
#endif
        }

        pool.deallocate(storage);
    }

    std::span<component_type> make_type_span(std::span<component_type, MaxComponentTypes> inOut, type_set current)
    {
        u32 count{0};

        for (usize i = 0; i < array_size(current.bitset); ++i)
        {
            const u64 v = current.bitset[i];

            if (v == 0)
            {
                continue;
            }

            u32 nextId = i * 32;

            // TODO: Could use bitscan reverse instead
            for (u64 mask = 1; mask != 0; mask <<= 1)
            {
                if (v & mask)
                {
                    inOut[count] = component_type{nextId};
                    ++count;
                }

                ++nextId;
            }
        }

        return inOut.subspan(0, count);
    }

    void reserve_chunks(memory_pool_impl& pool, archetype_storage& archetype, u32 newCount)
    {
        const u32 oldCount = archetype.numCurrentChunks;

        if (newCount <= oldCount)
        {
            return;
        }

        chunk** const newChunksArray = pool.create_array_uninitialized<chunk*>(newCount);

        if (archetype.chunks)
        {
            std::memcpy(newChunksArray, archetype.chunks, sizeof(chunk*) * oldCount);
            pool.deallocate_array(archetype.chunks, oldCount);
        }

        archetype.chunks = newChunksArray;
        archetype.numCurrentChunks = newCount;

        for (chunk **it = newChunksArray + oldCount, **end = newChunksArray + newCount; it != end; ++it)
        {
            chunk* const newChunk = pool.create_uninitialized<chunk>();
            *it = newChunk;

            newChunk->header = {};

            // Start lifetimes (possibly unnecessary in C++ 20?)
            new (get_entity_pointer(newChunk->data, 0)) entity[archetype.numEntitiesPerChunk];
            new (get_entity_tags_pointer(newChunk->data, archetype, 0)) entity_tags[archetype.numEntitiesPerChunk];
        }
    }
}
