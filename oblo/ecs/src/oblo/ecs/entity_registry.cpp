#include <oblo/ecs/entity_registry.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/ecs/component_type_desc.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/math/power_of_two.hpp>

#include <memory_resource>

namespace oblo::ecs
{
    namespace
    {
        static constexpr u32 ChunkSize{1u << 14};

        struct chunk
        {
            std::byte data[ChunkSize];
        };

        struct component_fn_table
        {
            create_fn create;
            destroy_fn destroy;
            move_fn move;
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
        };

        struct memory_pool_impl
        {
            std::pmr::unsynchronized_pool_resource poolResource;

            template <typename T>
            T* create_array_uninitialized(usize count)
            {
                void* vptr = poolResource.allocate(sizeof(T) * count, alignof(T));
                return new (vptr) T[count];
            }

            template <typename T>
            void create_array_uninitialized(T*& ptr, usize count)
            {
                ptr = create_array_uninitialized<T>(count);
            }

            template <typename T>
            void* allocate()
            {
                return poolResource.allocate(sizeof(T), alignof(T));
            }

            template <typename T>
            void deallocate(T* ptr)
            {
                poolResource.deallocate(ptr, sizeof(T), alignof(T));
            }

            template <typename T>
            void deallocate_array(T* ptr, usize count)
            {
                poolResource.deallocate(ptr, sizeof(T) * count, alignof(T));
            }
        };

        static_assert(std::is_trivially_destructible_v<archetype_storage>,
                      "We can avoid calling destructors in ~entity_manager if this is trivial");

        static_assert(MaxComponentTypes <= std::numeric_limits<decltype(archetype_storage::numComponents)>::max());

        archetype_storage* create_archetype_storage(memory_pool_impl& pool,
                                                    const type_registry& typeRegistry,
                                                    const type_set& signature,
                                                    std::span<const component_type> components)
        {
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
                };

                storage->alignments[componentIndex] = typeDesc.alignment;
                storage->sizes[componentIndex] = componentSize;

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

                    delete *it;
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

        entity* get_entity_pointer(std::byte* chunk, u32 offset)
        {
            return reinterpret_cast<entity*>(chunk) + offset;
        }

        std::byte* get_component_pointer(std::byte* chunk, archetype_storage& archetype, u8 componentIndex, u32 offset)
        {
            return chunk + archetype.offsets[componentIndex] + offset * archetype.sizes[componentIndex];
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
                *it = new chunk;
            }
        }
    }

    struct entity_registry::components_storage
    {
        archetype_storage* archetype;
    };

    struct entity_registry::tags_storage
    {
    };

    struct entity_registry::entity_data
    {
    };

    struct entity_registry::memory_pool : memory_pool_impl
    {
    };

    entity_registry::entity_registry() = default;

    entity_registry::entity_registry(const type_registry* typeRegistry) : m_typeRegistry{typeRegistry}
    {
        OBLO_ASSERT(m_typeRegistry);

        m_pool = std::make_unique<memory_pool>();
    }

    entity_registry::entity_registry(entity_registry&&) noexcept = default;

    entity_registry& entity_registry::operator=(entity_registry&& other) noexcept
    {
        this->~entity_registry();
        new (this) entity_registry{std::move(other)};
        return *this;
    }

    entity_registry::~entity_registry()
    {
        for (const auto& storage : m_componentsStorage)
        {
            destroy_archetype_storage(*m_pool, storage.archetype);
        }
    }

    void entity_registry::init(const type_registry* typeRegistry)
    {
        *this = entity_registry{typeRegistry};
    }

    entity entity_registry::create(const component_and_tags_sets& types, const u32 count)
    {
        if (count == 0)
        {
            return {};
        }

        const components_storage& storage = find_or_create_component_storage(types.components);
        archetype_storage* const archetype = storage.archetype;

        const u32 numEntitiesPerChunk = archetype->numEntitiesPerChunk;
        const u32 oldCount = archetype->numCurrentEntities;
        const u32 newCount = oldCount + count;
        const u32 numRequiredChunks = round_up_div(newCount, numEntitiesPerChunk);

        reserve_chunks(*m_pool, *archetype, numRequiredChunks);

        const auto res = m_nextId;

        const u32 availableInFirstChunk = numEntitiesPerChunk - oldCount % numEntitiesPerChunk;
        const u32 firstChunkIndex = oldCount / numEntitiesPerChunk;

        const u8 numComponents = archetype->numComponents;

        auto* const chunks = archetype->chunks;

        if (count < availableInFirstChunk)
        {
            std::byte* const chunkBytes = chunks[firstChunkIndex]->data;

            const u32 existingEntities = numEntitiesPerChunk - availableInFirstChunk;
            entity* const entities = get_entity_pointer(chunkBytes, existingEntities);

            for (entity *it = entities, *end = entities + count; it != end; ++it)
            {
                new (it) entity{m_nextId};
                ++m_nextId.value;
            }

            for (u8 componentIndex = 0; componentIndex != numComponents; ++componentIndex)
            {
                std::byte* const componentData =
                    get_component_pointer(chunkBytes, *archetype, componentIndex, existingEntities);

                archetype->fnTables[componentIndex].create(componentData, count);
            }
        }
        else
        {
            // TODO: Allocate over multiple chunks
            OBLO_ASSERT(false);
        }

        archetype->numCurrentEntities = newCount;

        return res;
    }

    const entity_registry::components_storage& entity_registry::find_or_create_component_storage(
        const type_set& components)
    {
        for (const auto& storage : m_componentsStorage)
        {
            if (storage.archetype->signature == components)
            {
                return storage;
            }
        }

        auto& newStorage = m_componentsStorage.emplace_back();

        component_type typeHandlesArray[MaxComponentTypes];

        const std::span typeHandles = make_type_span(typeHandlesArray, components);

        newStorage.archetype = create_archetype_storage(*m_pool, *m_typeRegistry, components, typeHandles);

        return newStorage;
    }

    void entity_registry::find_and_sort_component_types(std::span<const type_id> typeIds,
                                                        std::span<component_type> types)
    {
        OBLO_ASSERT(typeIds.size() == types.size());
        for (usize i = 0; i < typeIds.size(); ++i)
        {
            types[i] = m_typeRegistry->find_component(typeIds[i]);
        }
    }
}