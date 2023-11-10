#include <oblo/ecs/entity_registry.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/core/memory_pool.hpp>
#include <oblo/core/zip_range.hpp>
#include <oblo/ecs/archetype_impl.hpp>
#include <oblo/ecs/component_type_desc.hpp>
#include <oblo/ecs/range.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/ecs/type_set.hpp>

#include <algorithm>
#include <memory_resource>

namespace oblo::ecs
{
    static_assert(std::is_trivially_destructible_v<archetype_impl>,
        "We can avoid calling destructors in ~entity_manager if this is trivial");

    struct entity_registry::entity_data
    {
        archetype_impl* archetype;
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
            destroy_archetype_impl(*m_pool, storage.archetype);
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

        const archetype_storage& storage = find_or_create_storage(types);
        archetype_impl* const archetype = storage.archetype;

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

                archetype->fnTables[componentIndex].do_create(componentData, numEntitiesToCreate);
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

        move_last_and_pop(*entityData);

        m_entities.erase(e);
    }

    void entity_registry::add(entity e, const component_and_tags_sets& newTypes)
    {
        if (newTypes.components.is_empty() && newTypes.tags.is_empty())
        {
            return;
        }

        auto* const entityData = m_entities.try_find(e);

        if (!entityData)
        {
            return;
        }

        archetype_impl& oldArchetype = *entityData->archetype;
        const auto oldArchetypeIndex = entityData->archetypeIndex;

        component_and_tags_sets types = oldArchetype.types;
        types.components.add(newTypes.components);
        types.tags.add(newTypes.tags);

        const archetype_storage& newStorage = find_or_create_storage(types);
        archetype_impl& newArchetype = *newStorage.archetype;

        OBLO_ASSERT(oldArchetype.numCurrentEntities != 0);

        if (&newArchetype == &oldArchetype)
        {
            // Nothing to do
            return;
        }

        // First we get the entity index and move into the new archetype
        const auto [oldChunkIndex, oldChunkOffset] = get_entity_location(oldArchetype, oldArchetypeIndex);

        // Reserve space at the end of the new archetype
        const auto newArchetypeIndex = newArchetype.numCurrentEntities;

        const auto [newChunkIndex, newChunkOffset] = get_entity_location(newArchetype, newArchetypeIndex);

        reserve_chunks(*m_pool, newArchetype, newChunkIndex + 1);

        // TODO: Move assign old into new
        chunk* const oldChunk = oldArchetype.chunks[oldChunkIndex];
        chunk* const newChunk = newArchetype.chunks[newChunkIndex];

        u8 oldComponentIndex{0};

        for (u8 newComponentIndex = 0; newComponentIndex < newArchetype.numComponents; ++newComponentIndex)
        {
            auto* dst = get_component_pointer(newChunk->data, newArchetype, newComponentIndex, newChunkOffset);

            const bool isSameComponent = oldComponentIndex <= oldArchetype.numComponents &&
                oldArchetype.components[oldComponentIndex] == newArchetype.components[newComponentIndex];

            // If we have the old component, we can move it, otherwise we default construct a new one
            if (isSameComponent)
            {
                auto* src = get_component_pointer(oldChunk->data, oldArchetype, oldComponentIndex, oldChunkOffset);
                newArchetype.fnTables[newComponentIndex].do_move_assign(newArchetype.sizes[newComponentIndex],
                    dst,
                    src,
                    1);

                ++oldComponentIndex;
            }
            else
            {
                newArchetype.fnTables[newComponentIndex].do_create(dst, 1);
            }
        }

        // Update entity
        entity* const oldEntityPtr = get_entity_pointer(oldChunk->data, oldChunkOffset);
        entity* const newEntityPtr = get_entity_pointer(newChunk->data, newChunkOffset);
        *newEntityPtr = *oldEntityPtr;

        // Update tags
        entity_tags* const newTags = get_entity_tags_pointer(newChunk->data, newArchetype, newChunkOffset);
        *newTags = {newArchetype.types.tags};

        // Move last and pop (also decrements old archetype counters)
        move_last_and_pop(*entityData);

        // Update the references of the entity
        entityData->archetype = &newArchetype;
        entityData->archetypeIndex = newArchetypeIndex;

        // Update new chunk counters
        ++newChunk->header.numEntities;
        ++newArchetype.numCurrentEntities;
    }

    bool entity_registry::contains(entity e) const
    {
        return m_entities.try_find(e) != nullptr;
    }

    std::byte* entity_registry::try_get(entity e, component_type component)
    {
        std::byte* res{};

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

        return res;
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

    std::span<const entity> entity_registry::entities() const
    {
        return m_entities.keys();
    }

    std::span<const component_type> entity_registry::get_component_types(entity e) const
    {
        const entity_data* entityData = m_entities.try_find(e);

        if (!entityData)
        {
            return {};
        }

        auto* const archetype = entityData->archetype;
        return {archetype->components, archetype->numComponents};
    }

    const type_registry& entity_registry::get_type_registry() const
    {
        return *m_typeRegistry;
    }

    std::span<const archetype_storage> entity_registry::get_archetypes() const
    {
        return m_componentsStorage;
    }

    const archetype_storage* entity_registry::find_first_match(
        const archetype_storage* begin, usize increment, const component_and_tags_sets& types)
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

    bool entity_registry::fetch_component_offsets(
        const archetype_storage& storage, std::span<const component_type> componentTypes, std::span<u32> offsets)
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

    u32 entity_registry::fetch_chunk_data(const archetype_storage& storage,
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

    const archetype_storage& entity_registry::find_or_create_storage(const component_and_tags_sets& types)
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

        newStorage.archetype = create_archetype_impl(*m_pool, *m_typeRegistry, types, typeHandles);

        return newStorage;
    }

    void entity_registry::find_component_data(
        entity e, const std::span<const type_id> typeIds, std::span<std::byte*> outComponents) const
    {
        OBLO_ASSERT(typeIds.size() == outComponents.size());

        const entity_data* entityData = m_entities.try_find(e);
        auto* const archetype = entityData->archetype;

        if (!entityData)
        {
            for (auto& ptr : outComponents)
            {
                ptr = nullptr;
            }

            return;
        }

        for (usize i = 0; i < typeIds.size(); ++i)
        {
            const auto& typeId = typeIds[i];
            auto& ptr = outComponents[i];

            const auto component = m_typeRegistry->find_component(typeId);

            if (!entityData || !component)
            {
                ptr = nullptr;
                continue;
            }

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

    void entity_registry::move_last_and_pop(const entity_data& entityData)
    {
        auto& archetype = *entityData.archetype;
        const auto archetypeIndex = entityData.archetypeIndex;

        OBLO_ASSERT(archetype.numCurrentEntities != 0);

        const auto lastEntityArchetypeIndex = archetype.numCurrentEntities - 1;

        const auto [chunkIndex, chunkOffset] = get_entity_location(archetype, archetypeIndex);
        const auto [lastEntityChunkIndex, lastEntityChunkOffset] =
            get_entity_location(archetype, lastEntityArchetypeIndex);

        if (archetypeIndex != lastEntityArchetypeIndex)
        {
            // The entity is not the last, so we move the last into it to fill the hole
            chunk* const removedEntityChunk = archetype.chunks[chunkIndex];
            chunk* const lastEntityChunk = archetype.chunks[lastEntityChunkIndex];

            entity* const removedEntity = get_entity_pointer(removedEntityChunk->data, chunkOffset);
            entity* const lastEntity = get_entity_pointer(lastEntityChunk->data, lastEntityChunkOffset);

            entity_tags* const removedTags = get_entity_tags_pointer(removedEntityChunk->data, archetype, chunkOffset);
            entity_tags* const lastTags =
                get_entity_tags_pointer(lastEntityChunk->data, archetype, lastEntityChunkOffset);

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
                archetype.fnTables[componentIndex].do_move_assign(archetype.sizes[componentIndex], dst, src, 1);
                archetype.fnTables[componentIndex].do_destroy(src, 1);
            }

            --lastEntityChunk->header.numEntities;
        }
        else
        {
            // The entity is the last, all we need to do is popping it
            chunk* const removedEntityChunk = archetype.chunks[chunkIndex];
            chunk* const lastEntityChunk = archetype.chunks[lastEntityChunkIndex];

            for (u8 componentIndex = 0; componentIndex < archetype.numComponents; ++componentIndex)
            {
                auto* src = get_component_pointer(lastEntityChunk->data, archetype, componentIndex, chunkOffset);
                archetype.fnTables[componentIndex].do_destroy(src, 1);
            }

            --removedEntityChunk->header.numEntities;
        }

        // TODO: Could free pages if not used
        --archetype.numCurrentEntities;
    }

    component_and_tags_sets entity_registry::get_type_sets(entity e) const
    {
        auto* const entityData = m_entities.try_find(e);
        return entityData->archetype->types;
    }
}