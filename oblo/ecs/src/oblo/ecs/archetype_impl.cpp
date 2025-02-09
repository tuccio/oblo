#include <oblo/ecs/archetype_impl.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/core/iterator/zip_range.hpp>
#include <oblo/core/memory_pool.hpp>
#include <oblo/ecs/archetype_storage.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/math/power_of_two.hpp>

#include <algorithm>

namespace oblo::ecs
{
    static_assert(sizeof(chunk) == ChunkWithHeaderSize);
    static_assert(MaxComponentTypes <= std::numeric_limits<decltype(archetype_impl::numComponents)>::max());

    namespace
    {
        template <typename T, usize N>
        std::span<T> make_type_span(std::span<T, N> inOut, type_set current)
        {
            u32 count{0};

            for (usize i = 0; i < array_size(current.bitset); ++i)
            {
                const u64 v = current.bitset[i];

                if (v == 0)
                {
                    continue;
                }

                u32 nextId = u32(i * 32);

                // TODO: Could use bitscan reverse instead
                for (u64 mask = 1; mask != 0; mask <<= 1)
                {
                    if (v & mask)
                    {
                        inOut[count] = T{nextId};
                        ++count;
                    }

                    ++nextId;
                }
            }

            return inOut.subspan(0, count);
        }
    }

    archetype_impl* create_archetype_impl(
        memory_pool& pool, const type_registry& typeRegistry, const component_and_tag_sets& types)
    {
        component_type componentTypeHandlesArray[MaxComponentTypes];
        tag_type tagTypeHandlesArray[MaxTagTypes];

        const std::span components = make_type_span(std::span{componentTypeHandlesArray}, types.components);
        const std::span tags = make_type_span(std::span{tagTypeHandlesArray}, types.tags);

        OBLO_ASSERT(std::is_sorted(components.begin(), components.end()));
        const u8 numComponents = u8(components.size());
        const u8 numTags = u8(tags.size());

        archetype_impl* storage = new (pool.allocate<archetype_impl>()) archetype_impl{
            .types = types,
            .numComponents = numComponents,
            .numTags = numTags,
        };

        pool.create_array_uninitialized(storage->components, numComponents);
        pool.create_array_uninitialized(storage->tags, numTags);
        pool.create_array_uninitialized(storage->offsets, numComponents);
        pool.create_array_uninitialized(storage->sizes, numComponents);
        pool.create_array_uninitialized(storage->alignments, numComponents);
        pool.create_array_uninitialized(storage->fnTables, numComponents);

#ifdef OBLO_DEBUG
        pool.create_array_uninitialized(storage->typeIds, numComponents);
#endif

        std::memcpy(storage->components, components.data(), components.size_bytes());
        std::memcpy(storage->tags, tags.data(), tags.size_bytes());

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

                previousAlignment = typeDesc.alignment;
            }
        }

        const u32 numEntitiesPerChunk = u32((ChunkSize - paddingWorstCase) / columnsSizeSum);
        storage->numEntitiesPerChunk = numEntitiesPerChunk;

        u32 currentOffset = 0;

        const auto alignOffset = [](u32 currentOffset, usize newAlignment)
        {
            OBLO_ASSERT(newAlignment <= PageAlignment);
            return align_power_of_two(currentOffset, u32(newAlignment));
        };

        // First we have entity ids
        currentOffset = alignOffset(currentOffset, alignof(entity));
        currentOffset += u32(+sizeof(entity) * numEntitiesPerChunk);

        // Then we have the tags
        storage->entityTagsOffset = currentOffset;

        currentOffset = alignOffset(currentOffset, alignof(entity));
        currentOffset += u32(sizeof(entity_tags) * numEntitiesPerChunk);

        for (u8 componentIndex = 0; componentIndex < numComponents; ++componentIndex)
        {
            const auto size = storage->sizes[componentIndex];
            const auto alignment = storage->alignments[componentIndex];

            const u32 startOffset = alignOffset(currentOffset, alignment);
            OBLO_ASSERT(startOffset % alignment == 0);

            storage->offsets[componentIndex] = startOffset;

            currentOffset = startOffset + size * numEntitiesPerChunk;
            OBLO_ASSERT(currentOffset <= ChunkSize);
        }

        return storage;
    }

    void destroy_archetype_impl(memory_pool& pool, archetype_impl* storage)
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
                        storage->fnTables[componentIndex].do_destroy(componentData, numEntitiesInChunk);
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
            pool.deallocate_array(storage->tags, storage->numTags);
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

    void reserve_chunks(memory_pool& pool, archetype_impl& archetype, u32 newCount)
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

    component_and_tag_sets get_component_and_tag_sets(const archetype_storage& storage)
    {
        return storage.archetype->types;
    }

    std::span<const component_type> get_component_types(const archetype_storage& storage)
    {
        return {storage.archetype->components, storage.archetype->numComponents};
    }

    std::span<const tag_type> get_tag_types(const archetype_storage& storage)
    {
        return {storage.archetype->tags, storage.archetype->numTags};
    }

    u32 get_used_chunks_count(const archetype_storage& storage)
    {
        return round_up_div(storage.archetype->numCurrentEntities, storage.archetype->numEntitiesPerChunk);
    }

    u32 get_entities_count(const archetype_storage& storage)
    {
        return storage.archetype->numCurrentEntities;
    }

    u32 get_entities_count_in_chunk(const archetype_storage& storage, u32 chunkIndex)
    {
        return storage.archetype->chunks[chunkIndex]->header.numEntities;
    }

    u64* access_archetype_modification_id(const archetype_storage& storage)
    {
        return &storage.archetype->modificationId;
    }

    u64* access_chunk_modification_id(const archetype_storage& storage, u32 chunkIndex)
    {
        return &storage.archetype->chunks[chunkIndex]->header.modificationId;
    }

    void fetch_component_offsets(
        const archetype_storage& storage, std::span<const component_type> componentTypes, std::span<u32> offsets)
    {
        OBLO_ASSERT(offsets.size() == componentTypes.size());

        const auto& archetype = *storage.archetype;
        const auto* archetypeTypeIt = archetype.components;

        auto outIt = offsets.begin();

        for (auto it = componentTypes.begin(); it != componentTypes.end();)
        {
            while (*it != *archetypeTypeIt)
            {
                if (*it < *archetypeTypeIt)
                {
                    ++it;
                }
                else
                {
                    ++archetypeTypeIt;
                }
            }

            const u8 componentIndex = u8(archetypeTypeIt - archetype.components);

            *outIt = archetype.offsets[componentIndex];

            ++archetypeTypeIt;
            ++it;
            ++outIt;
        }
    }

    u32 fetch_chunk_data(const archetype_storage& storage,
        u32 chunkIndex,
        std::span<const u32> offsets,
        const entity** entities,
        std::span<std::byte*> componentData)
    {
        const auto& archetype = *storage.archetype;
        chunk* const chunk = archetype.chunks[chunkIndex];

        *entities = get_entity_pointer(chunk->data, 0);

        for (auto&& [offset, ptr] : zip_range(offsets, componentData))
        {
            ptr = chunk->data + offset;
        }

        return chunk->header.numEntities;
    }
}
