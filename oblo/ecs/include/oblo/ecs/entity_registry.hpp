#pragma once

#include <oblo/core/flat_dense_map.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/ecs/handles.hpp>
#include <oblo/ecs/traits.hpp>

#include <memory>

namespace oblo::ecs
{
    class type_registry;
    struct component_and_tags_sets;
    struct type_set;

    class entity_registry final
    {
    public:
        template <typename... Components>
        class typed_range;

        entity_registry();
        explicit entity_registry(const type_registry* typeRegistry);
        entity_registry(const entity_registry&) = delete;
        entity_registry(entity_registry&&) noexcept;
        entity_registry& operator=(const entity_registry&) = delete;
        entity_registry& operator=(entity_registry&&) noexcept;
        ~entity_registry();

        void init(const type_registry* typeRegistry);

        entity create(const component_and_tags_sets& types, u32 count = 1);

        template <typename... ComponentsOrTags>
        entity create(u32 count = 1);

        void destroy(entity e);

        void add(entity e, const component_and_tags_sets& types);

        template <typename... ComponentsOrTags>
        void add(entity e);

        void remove(entity e, const component_and_tags_sets& types);

        template <typename... ComponentsOrTags>
        void remove(entity e);

        bool contains(entity e) const;

        template <typename Component>
        const Component& get(entity e) const;

        template <typename Component>
        Component& get(entity e);

        // Requires including oblo/ecs/range.hpp
        template <typename... Components>
        typed_range<Components...> range();

    private:
        struct components_storage;
        struct memory_pool;
        struct tags_storage;
        struct entity_data;

    private:
        const components_storage* find_first_match(const components_storage* begin,
                                                   usize increment,
                                                   const type_set& components);

        static void sort_and_map(std::span<component_type> componentTypes, std::span<u8> mapping);

        static u32 get_used_chunks_count(const components_storage& storage);

        static bool fetch_component_offsets(const components_storage& storage,
                                            std::span<const component_type> componentTypes,
                                            std::span<u32> offsets);

        static u32 fetch_chunk_data(const components_storage& storage,
                                    u32 chunkIndex,
                                    u32 numUsedChunks,
                                    std::span<const u32> offsets,
                                    const entity** entities,
                                    std::span<std::byte*> componentData);

        const components_storage& find_or_create_component_storage(const type_set& components);

        void find_component_types(std::span<const type_id> typeIds, std::span<component_type> types);

        std::byte* find_component_data(entity e, const type_id& typeId) const;

    private:
        const type_registry* m_typeRegistry{nullptr};
        std::unique_ptr<memory_pool> m_pool;
        flat_dense_map<entity, entity_data> m_entities;
        std::vector<components_storage> m_componentsStorage;
        tags_storage* m_tagsStorage;
        entity m_nextId{1};
    };

    template <typename... ComponentsOrTags>
    entity entity_registry::create(u32 count)
    {
        return create(make_type_sets<ComponentsOrTags...>(*m_typeRegistry), count);
    }

    template <typename Component>
    const Component& entity_registry::get(entity e) const
    {
        std::byte* const p = find_component_data(e, get_type_id<Component>());
        OBLO_ASSERT(p);
        return *reinterpret_cast<const Component*>(p);
    }

    template <typename Component>
    Component& entity_registry::get(entity e)
    {
        std::byte* const p = find_component_data(e, get_type_id<Component>());
        OBLO_ASSERT(p);
        return *reinterpret_cast<Component*>(p);
    }
}