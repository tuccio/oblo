#include <oblo/ecs/archetype_storage.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/ecs/memory_pool_impl.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/math/power_of_two.hpp>

#include <algorithm>

namespace oblo::ecs
{
    archetype_storage* create_archetype_storage(memory_pool_impl& pool,
                                                const type_registry& typeRegistry,
                                                const type_set& signature,
                                                std::span<const component_type> components)
    {
        OBLO_ASSERT(std::is_sorted(components.begin(), components.end()));
        const u8 numComponents = u8(components.size());

        archetype_storage* storage = new (pool.allocate<archetype_storage>()) archetype_storage{
            .signature = signature,
            .numComponents = numComponents,
        };

        pool.create_array_uninitialized(storage->components, numComponents);
        pool.create_array_uninitialized(storage->offsets, numComponents);
        pool.create_array_uninitialized(storage->sizes, numComponents);
        pool.create_array_uninitialized(storage->alignments, numComponents);
        pool.create_array_uninitialized(storage->fnTables, numComponents);

#if OBLO_ECS_DEBUG_DATA
        pool.create_array_uninitialized(storage->typeIds, numComponents);
#endif

        std::memcpy(storage->components, components.data(), components.size_bytes());

        usize columnsSizeSum = sizeof(entity);
        usize paddingWorstCase = 0;

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

#if OBLO_ECS_DEBUG_DATA
            storage->typeIds[componentIndex] = typeDesc.type;
#endif

            columnsSizeSum += componentSize;
            paddingWorstCase += typeDesc.alignment - 1;
        }

        const u32 numEntitiesPerChunk = (ChunkSize - paddingWorstCase) / columnsSizeSum;
        u32 currentOffset = 0;

        storage->numEntitiesPerChunk = numEntitiesPerChunk;

        // The initial page alignment
        usize previousAlignment = alignof(std::max_align_t);

        const auto computePadding = [&previousAlignment](usize newAlignment)
        { return previousAlignment >= newAlignment ? usize(0) : newAlignment - previousAlignment; };

        // First we have entity ids
        currentOffset += computePadding(alignof(entity)) + sizeof(entity) * numEntitiesPerChunk;
        previousAlignment = alignof(entity);

        for (u8 componentIndex = 0; componentIndex < numComponents; ++componentIndex)
        {
            const auto size = storage->sizes[componentIndex];
            const auto alignment = storage->alignments[componentIndex];
            const auto padding = computePadding(alignment);

            storage->offsets[componentIndex] = currentOffset;

            currentOffset += padding + size * numEntitiesPerChunk;
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

#if OBLO_ECS_DEBUG_DATA
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
            *it = pool.create_uninitialized<chunk>();
        }
    }
}
