#include <oblo/ecs/entity_registry.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/core/zip_range.hpp>
#include <oblo/ecs/archetype_storage.hpp>
#include <oblo/ecs/component_type_desc.hpp>
#include <oblo/ecs/memory_pool_impl.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/ecs/type_set.hpp>

#include <algorithm>
#include <memory_resource>

namespace oblo::ecs
{
    static_assert(std::is_trivially_destructible_v<archetype_storage>,
                  "We can avoid calling destructors in ~entity_manager if this is trivial");
    namespace
    {
        template <typename T>
        struct pooled_array
        {
            static constexpr u32 MinAllocation{16};
            static constexpr f32 GrowthFactor{1.6f};

            u32 size;
            u32 capacity;
            T* data;

            void resize_and_grow(memory_pool_impl& pool, u32 newSize)
            {
                OBLO_ASSERT(newSize >= size);

                if (newSize <= capacity)
                {
                    return;
                }

                const u32 newCapacity = max(MinAllocation, u32(capacity * GrowthFactor));

                T* const newArray = pool.create_array_uninitialized<T>(newCapacity);
                std::copy_n(data, size, newArray);

                pool.deallocate_array(data);

                data = newArray;
                capacity = newCapacity;
                size = newSize;
            }

            void free(memory_pool_impl& pool)
            {
                pool.deallocate_array(data);
                *this = {};
            }
        };

        struct per_tag_data
        {
            pooled_array<entity> entities{};
        };
    }

    struct entity_registry::components_storage
    {
        archetype_storage* archetype;
    };

    struct entity_registry::tags_storage
    {
        pooled_array<per_tag_data> tags{};
    };

    struct entity_registry::entity_data
    {
        archetype_storage* archetype;
        u32 archetypeIndex;
    };

    struct entity_registry::memory_pool : memory_pool_impl
    {
    };

    entity_registry::entity_registry() = default;

    entity_registry::entity_registry(const type_registry* typeRegistry) : m_typeRegistry{typeRegistry}
    {
        OBLO_ASSERT(m_typeRegistry);

        m_pool = std::make_unique<memory_pool>();
        m_tagsStorage = new (m_pool->allocate<tags_storage>()) tags_storage{};
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

        const entity firstCreatedEntityId = m_nextId;

        chunk** const chunks = archetype->chunks;
        const u32 firstChunkIndex = oldCount / numEntitiesPerChunk;
        const u8 numComponents = archetype->numComponents;

        u32 numEntitiesInCurrentChunk = oldCount % numEntitiesPerChunk;
        u32 numRemainingEntities = count;

        u32 archetypeIndex = oldCount;

        for (chunk** chunk = chunks + firstChunkIndex; chunk != chunks + numRequiredChunks;
             ++chunk, numEntitiesInCurrentChunk = 0)
        {
            std::byte* const chunkBytes = (*chunk)->data;

            entity* const entities = get_entity_pointer(chunkBytes, numEntitiesInCurrentChunk);

            const u32 numEntitiesToCreate = min(numRemainingEntities, numEntitiesPerChunk - numEntitiesInCurrentChunk);

            for (entity *it = entities, *end = entities + numEntitiesToCreate; it != end; ++it)
            {
                m_entities.emplace(m_nextId, archetype, archetypeIndex);
                ++archetypeIndex;

                new (it) entity{m_nextId};
                ++m_nextId.value;
            }

            for (u8 componentIndex = 0; componentIndex != numComponents; ++componentIndex)
            {
                std::byte* const componentData =
                    get_component_pointer(chunkBytes, *archetype, componentIndex, numEntitiesInCurrentChunk);

                archetype->fnTables[componentIndex].create(componentData, numEntitiesToCreate);
            }

            numRemainingEntities -= numEntitiesToCreate;
        }

        archetype->numCurrentEntities = newCount;

        return firstCreatedEntityId;
    }

    void entity_registry::destroy(entity e)
    {
        const auto* entityData = m_entities.try_find(e);

        if (!entityData)
        {
            return;
        }

        auto& archetype = *entityData->archetype;
        const auto archetypeIndex = entityData->archetypeIndex;

        OBLO_ASSERT(archetype.numCurrentEntities != 0);

        const auto [chunkIndex, chunkOffset] = get_entity_location(archetype, archetypeIndex);
        const auto [lastEntityChunkIndex, lastEntityChunkOffset] =
            get_entity_location(archetype, archetype.numCurrentEntities - 1);

        if (chunkIndex != lastEntityChunkIndex || chunkOffset != lastEntityChunkOffset)
        {
            chunk* const removedEntityChunk = archetype.chunks[chunkIndex];
            chunk* const lastEntityChunk = archetype.chunks[lastEntityChunkIndex];

            entity* const removedEntity = get_entity_pointer(removedEntityChunk->data, chunkOffset);
            entity* const lastEntity = get_entity_pointer(lastEntityChunk->data, lastEntityChunkOffset);

            // We will swap the removed entity with the last, so we remove the archetype index
            m_entities.erase(*removedEntity);
            auto* const lastEntityData = m_entities.try_find(*lastEntity);
            OBLO_ASSERT(lastEntityData);
            lastEntityData->archetypeIndex = archetypeIndex;

            std::swap(*removedEntity, *lastEntity);

            for (u8 componentIndex = 0; componentIndex < archetype.numComponents; ++componentIndex)
            {
                auto* dst = get_component_pointer(removedEntityChunk->data, archetype, componentIndex, chunkOffset);
                auto* src =
                    get_component_pointer(lastEntityChunk->data, archetype, componentIndex, lastEntityChunkOffset);
                archetype.fnTables[componentIndex].moveAssign(dst, src, 1);
                archetype.fnTables[componentIndex].destroy(src, 1);
            }
        }
        else
        {
            chunk* const removedEntityChunk = archetype.chunks[chunkIndex];
            entity* const removedEntity = get_entity_pointer(removedEntityChunk->data, chunkOffset);
            m_entities.erase(*removedEntity);

            chunk* const lastEntityChunk = archetype.chunks[lastEntityChunkIndex];

            for (u8 componentIndex = 0; componentIndex < archetype.numComponents; ++componentIndex)
            {
                auto* src = get_component_pointer(lastEntityChunk->data, archetype, componentIndex, chunkOffset);
                archetype.fnTables[componentIndex].destroy(src, 1);
            }
        }

        // TODO: Could free pages if not used
        --archetype.numCurrentEntities;
    }

    bool entity_registry::contains(entity e) const
    {
        return m_entities.try_find(e) != nullptr;
    }

    const entity_registry::components_storage* entity_registry::find_first_match(const components_storage* begin,
                                                                                 usize increment,
                                                                                 const type_set& components)
    {
        auto* const end = m_componentsStorage.data() + m_componentsStorage.size();

        for (auto* it = begin + increment; it != end; ++it)
        {
            const auto intersection = it->archetype->signature.intersection(components);

            if (intersection == components && it->archetype->numCurrentEntities != 0)
            {
                return it;
            }
        }

        return nullptr;
    }

    void entity_registry::sort_and_map(const std::span<component_type> componentTypes, const std::span<u8> mapping)
    {
        for (u8 i = 0; i < mapping.size(); ++i)
        {
            mapping[i] = i;
        }

        const auto zip = zip_range(componentTypes, mapping);

        std::sort(zip.begin(),
                  zip.end(),
                  [](const auto& lhs, const auto& rhs) { return std::get<0>(lhs) < std::get<0>(rhs); });
    }

    u32 entity_registry::get_used_chunks_count(const components_storage& storage)
    {
        return round_up_div(storage.archetype->numCurrentEntities, storage.archetype->numEntitiesPerChunk);
    }

    bool entity_registry::fetch_component_offsets(const components_storage& storage,
                                                  std::span<const component_type> componentTypes,
                                                  std::span<u32> offsets)
    {
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

        return true;
    }

    u32 entity_registry::fetch_chunk_data(const components_storage& storage,
                                          u32 chunkIndex,
                                          u32 numUsedChunks,
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

        return chunkIndex == numUsedChunks - 1 ? archetype.numCurrentEntities % archetype.numEntitiesPerChunk
                                               : archetype.numEntitiesPerChunk;
    }

    void entity_registry::find_component_types(std::span<const type_id> typeIds, std::span<component_type> types)
    {
        OBLO_ASSERT(typeIds.size() == types.size());
        for (usize i = 0; i < typeIds.size(); ++i)
        {
            types[i] = m_typeRegistry->find_component(typeIds[i]);
        }
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

    std::byte* entity_registry::find_component_data(entity e, const type_id& typeId) const
    {
        const auto component = m_typeRegistry->find_component(typeId);
        const entity_data* entityData = m_entities.try_find(e);

        if (!entityData || !component)
        {
            return nullptr;
        }

        auto* const archetype = entityData->archetype;

        const u8 componentIndex = find_component_index({archetype->components, archetype->numComponents}, component);

        if (componentIndex == InvalidComponentIndex)
        {
            return nullptr;
        }

        const auto [chunkIndex, chunkOffset] = get_entity_location(*archetype, entityData->archetypeIndex);

        return get_component_pointer(archetype->chunks[chunkIndex]->data, *archetype, componentIndex, chunkOffset);
    }
}