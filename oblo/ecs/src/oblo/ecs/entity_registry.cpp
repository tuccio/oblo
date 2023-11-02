#include <oblo/ecs/entity_registry.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/core/memory_pool.hpp>
#include <oblo/core/zip_range.hpp>
#include <oblo/ecs/archetype_storage.hpp>
#include <oblo/ecs/component_type_desc.hpp>
#include <oblo/ecs/range.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/ecs/type_set.hpp>

#include <algorithm>
#include <memory_resource>

namespace oblo::ecs
{
    static_assert(std::is_trivially_destructible_v<archetype_storage>,
        "We can avoid calling destructors in ~entity_manager if this is trivial");

    struct entity_registry::components_storage
    {
        archetype_storage* archetype;
    };

    struct entity_registry::entity_data
    {
        archetype_storage* archetype;
        u32 archetypeIndex;
    };

    struct entity_registry::memory_pool : oblo::memory_pool
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

    entity entity_registry::create(const component_and_tags_sets& types)
    {
        entity e;
        create(types, 1, {&e, 1});
        return e;
    }

    void entity_registry::create(const component_and_tags_sets& types, const u32 count, std::span<entity> outEntityIds)
    {
        OBLO_ASSERT(outEntityIds.size() == 0 || outEntityIds.size() == count);

        if (count == 0)
        {
            return;
        }

        const components_storage& storage = find_or_create_storage(types);
        archetype_storage* const archetype = storage.archetype;

        const u32 numEntitiesPerChunk = archetype->numEntitiesPerChunk;
        const u32 oldCount = archetype->numCurrentEntities;
        const u32 newCount = oldCount + count;
        const u32 numRequiredChunks = round_up_div(newCount, numEntitiesPerChunk);

        reserve_chunks(*m_pool, *archetype, numRequiredChunks);

        chunk** const chunks = archetype->chunks;
        const u32 firstChunkIndex = oldCount / numEntitiesPerChunk;
        const u8 numComponents = archetype->numComponents;

        u32 numEntitiesInCurrentChunk = oldCount % numEntitiesPerChunk;
        u32 numRemainingEntities = count;

        u32 archetypeIndex = oldCount;
        auto outIt = outEntityIds.begin();

        for (chunk** chunk = chunks + firstChunkIndex; chunk != chunks + numRequiredChunks;
             ++chunk, numEntitiesInCurrentChunk = 0)
        {
            std::byte* const chunkBytes = (*chunk)->data;

            entity* const entities = get_entity_pointer(chunkBytes, numEntitiesInCurrentChunk);

            const u32 numEntitiesToCreate = min(numRemainingEntities, numEntitiesPerChunk - numEntitiesInCurrentChunk);

            for (entity *it = entities, *end = entities + numEntitiesToCreate; it != end; ++it)
            {
                const auto [_, entityId] = m_entities.emplace(archetype, archetypeIndex);
                ++archetypeIndex;

                new (it) entity{entityId};

                if (outIt != outEntityIds.end())
                {
                    *outIt = entityId;
                    ++outIt;
                }
            }

            std::fill_n(get_entity_tags_pointer(chunkBytes, *archetype, 0),
                numEntitiesToCreate,
                entity_tags{.types = types.tags});

            for (u8 componentIndex = 0; componentIndex != numComponents; ++componentIndex)
            {
                std::byte* const componentData =
                    get_component_pointer(chunkBytes, *archetype, componentIndex, numEntitiesInCurrentChunk);

                archetype->fnTables[componentIndex].create(componentData, numEntitiesToCreate);
            }

            (*chunk)->header.numEntities += numEntitiesToCreate;

            numRemainingEntities -= numEntitiesToCreate;
        }

        archetype->numCurrentEntities = newCount;
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

            entity_tags* const removedTags = get_entity_tags_pointer(removedEntityChunk->data, archetype, chunkOffset);
            entity_tags* const lastTags =
                get_entity_tags_pointer(lastEntityChunk->data, archetype, lastEntityChunkOffset);

            // We will swap the removed entity with the last, so we remove the archetype index
            m_entities.erase(*removedEntity);

            auto* const lastEntityData = m_entities.try_find(*lastEntity);
            OBLO_ASSERT(lastEntityData);
            lastEntityData->archetypeIndex = archetypeIndex;

            *removedEntity = *lastEntity;
            *removedTags = *lastTags;

            for (u8 componentIndex = 0; componentIndex < archetype.numComponents; ++componentIndex)
            {
                auto* dst = get_component_pointer(removedEntityChunk->data, archetype, componentIndex, chunkOffset);
                auto* src =
                    get_component_pointer(lastEntityChunk->data, archetype, componentIndex, lastEntityChunkOffset);
                archetype.fnTables[componentIndex].moveAssign(dst, src, 1);
                archetype.fnTables[componentIndex].destroy(src, 1);
            }

            --lastEntityChunk->header.numEntities;
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

            --removedEntityChunk->header.numEntities;
        }

        // TODO: Could free pages if not used
        --archetype.numCurrentEntities;
    }

    bool entity_registry::contains(entity e) const
    {
        return m_entities.try_find(e) != nullptr;
    }

    void entity_registry::get(
        entity e, const std::span<const component_type> components, std::span<const std::byte*> outComponents) const
    {
        OBLO_ASSERT(components.size() == outComponents.size());

        for (usize i = 0; i < components.size(); ++i)
        {
            auto& ptr = outComponents[i];

            const auto component = components[i];
            const entity_data* entityData = m_entities.try_find(e);

            if (!entityData || !component)
            {
                ptr = nullptr;
                continue;
            }

            auto* const archetype = entityData->archetype;

            const u8 componentIndex =
                find_component_index({archetype->components, archetype->numComponents}, component);

            if (componentIndex == InvalidComponentIndex)
            {
                ptr = nullptr;
                continue;
            }

            const auto [chunkIndex, chunkOffset] = get_entity_location(*archetype, entityData->archetypeIndex);

            ptr = get_component_pointer(archetype->chunks[chunkIndex]->data, *archetype, componentIndex, chunkOffset);
        }
    }

    void entity_registry::get(
        entity e, const std::span<const component_type> components, std::span<std::byte*> outComponents)
    {
        get(e, components, {const_cast<const std::byte**>(outComponents.data()), outComponents.size()});
    }

    const entity_registry::components_storage* entity_registry::find_first_match(
        const components_storage* begin, usize increment, const component_and_tags_sets& types)
    {
        auto* const end = m_componentsStorage.data() + m_componentsStorage.size();

        for (auto* it = begin + increment; it != end; ++it)
        {
            const auto compInt = it->archetype->types.components.intersection(types.components);
            const auto tagsInt = it->archetype->types.tags.intersection(types.tags);

            if (compInt == types.components && tagsInt == types.tags && it->archetype->numCurrentEntities != 0)
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

    bool entity_registry::fetch_component_offsets(
        const components_storage& storage, std::span<const component_type> componentTypes, std::span<u32> offsets)
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

    void entity_registry::find_component_types(std::span<const type_id> typeIds, std::span<component_type> types)
    {
        OBLO_ASSERT(typeIds.size() == types.size());
        for (usize i = 0; i < typeIds.size(); ++i)
        {
            types[i] = m_typeRegistry->find_component(typeIds[i]);
        }
    }

    const entity_registry::components_storage& entity_registry::find_or_create_storage(
        const component_and_tags_sets& types)
    {
        for (const auto& storage : m_componentsStorage)
        {
            if (storage.archetype->types == types)
            {
                return storage;
            }
        }

        auto& newStorage = m_componentsStorage.emplace_back();

        component_type typeHandlesArray[MaxComponentTypes];

        const std::span typeHandles = make_type_span(typeHandlesArray, types.components);

        newStorage.archetype = create_archetype_storage(*m_pool, *m_typeRegistry, types, typeHandles);

        return newStorage;
    }

    void entity_registry::find_component_data(
        entity e, const std::span<const type_id> typeIds, std::span<std::byte*> outComponents) const
    {
        OBLO_ASSERT(typeIds.size() == outComponents.size());

        for (usize i = 0; i < typeIds.size(); ++i)
        {
            const auto& typeId = typeIds[i];
            auto& ptr = outComponents[i];

            const auto component = m_typeRegistry->find_component(typeId);
            const entity_data* entityData = m_entities.try_find(e);

            if (!entityData || !component)
            {
                ptr = nullptr;
                continue;
            }

            auto* const archetype = entityData->archetype;

            const u8 componentIndex =
                find_component_index({archetype->components, archetype->numComponents}, component);

            if (componentIndex == InvalidComponentIndex)
            {
                ptr = nullptr;
                continue;
            }

            const auto [chunkIndex, chunkOffset] = get_entity_location(*archetype, entityData->archetypeIndex);

            ptr = get_component_pointer(archetype->chunks[chunkIndex]->data, *archetype, componentIndex, chunkOffset);
        }
    }
}