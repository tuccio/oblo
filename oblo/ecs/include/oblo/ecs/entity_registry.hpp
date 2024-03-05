#pragma once

#include <oblo/core/handle_flat_pool_map.hpp>
#include <oblo/core/handle_pool.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/ecs/handles.hpp>
#include <oblo/ecs/traits.hpp>
#include <oblo/ecs/type_set.hpp>

#include <memory>
#include <tuple>

namespace oblo::ecs
{
    class type_registry;
    struct archetype_storage;
    struct component_and_tag_sets;
    struct type_set;

    class entity_registry final
    {
    public:
        template <typename... Components>
        class typed_range;

        entity_registry();
        explicit entity_registry(type_registry* typeRegistry);
        entity_registry(const entity_registry&) = delete;
        entity_registry(entity_registry&&) noexcept;
        entity_registry& operator=(const entity_registry&) = delete;
        entity_registry& operator=(entity_registry&&) noexcept;
        ~entity_registry();

        void init(type_registry* typeRegistry);

        entity create(const component_and_tag_sets& types);
        void create(const component_and_tag_sets& types, u32 count, std::span<entity> outEntityIds = {});

        template <typename... ComponentsOrTags>
        entity create();

        template <typename... ComponentsOrTags>
        void create(u32 count, std::span<entity> outEntityIds = {});

        void destroy(entity e);

        void add(entity e, const component_and_tag_sets& types);

        template <typename... ComponentsOrTags>
        decltype(auto) add(entity e);

        void remove(entity e, const component_and_tag_sets& types);

        template <typename... ComponentsOrTags>
        void remove(entity e);

        bool contains(entity e) const;

        template <typename Component>
        const Component& get(entity e) const;

        template <typename Component>
        Component& get(entity e);

        template <typename Component>
        const Component* try_get(entity e) const;

        template <typename Component>
        Component* try_get(entity e);

        std::byte* try_get(entity e, component_type component);

        void get(entity e,
            const std::span<const component_type> components,
            std::span<const std::byte*> outComponents) const;

        void get(entity e, const std::span<const component_type> components, std::span<std::byte*> outComponents);

        template <typename... Components>
            requires(sizeof...(Components) > 1)
        std::tuple<const Components&...> get(entity e) const;

        template <typename... Components>
            requires(sizeof...(Components) > 1)
        std::tuple<Components&...> get(entity e);

        template <typename... ComponentsOrTags>
        bool has(entity e) const;

        // Requires including oblo/ecs/range.hpp
        template <typename... Components>
        typed_range<Components...> range();

        std::span<const entity> entities() const;

        std::span<const component_type> get_component_types(entity e) const;

        type_registry& get_type_registry() const;

        std::span<const archetype_storage> get_archetypes() const;

    private:
        struct memory_pool;
        struct tags_storage;
        struct entity_data;

    private:
        const archetype_storage* find_first_match(const archetype_storage* begin,
            usize increment,
            const component_and_tag_sets& includes,
            const component_and_tag_sets& excludes);

        static void sort_and_map(std::span<component_type> componentTypes, std::span<u8> mapping);

        static bool fetch_component_offsets(
            const archetype_storage& storage, std::span<const component_type> componentTypes, std::span<u32> offsets);

        static u32 fetch_chunk_data(const archetype_storage& storage,
            u32 chunkIndex,
            std::span<const u32> offsets,
            const entity** entities,
            std::span<std::byte*> componentData);

        const archetype_storage& find_or_create_storage(const component_and_tag_sets& types);

        void find_component_types(std::span<const type_id> typeIds, std::span<component_type> types);

        void find_component_data(
            entity e, const std::span<const type_id> typeIds, std::span<std::byte*> outComponents) const;

        void move_last_and_pop(const entity_data& entityData);

        component_and_tag_sets get_type_sets(entity e) const;

    private:
        using entities_map = h32_flat_pool_dense_map<entity_handle, entity_data>;

        type_registry* m_typeRegistry{nullptr};
        std::unique_ptr<memory_pool> m_pool;
        entities_map m_entities;
        std::vector<archetype_storage> m_componentsStorage;
    };

    template <typename... ComponentsOrTags>
    entity entity_registry::create()
    {
        return create(make_type_sets<ComponentsOrTags...>(*m_typeRegistry));
    }

    template <typename... ComponentsOrTags>
    void entity_registry::create(u32 count, std::span<entity> outEntityIds)
    {
        return create(make_type_sets<ComponentsOrTags...>(*m_typeRegistry), count, outEntityIds);
    }

    template <typename Component>
    const Component& entity_registry::get(entity e) const
    {
        constexpr type_id types[] = {get_type_id<Component>()};
        std::byte* pointers[1];

        find_component_data(e, types, pointers);
        OBLO_ASSERT(pointers[0]);

        return *reinterpret_cast<const Component*>(pointers[0]);
    }

    template <typename Component>
    Component& entity_registry::get(entity e)
    {
        constexpr type_id types[] = {get_type_id<Component>()};
        std::byte* pointers[1];

        find_component_data(e, types, pointers);
        OBLO_ASSERT(pointers[0]);

        return *reinterpret_cast<Component*>(pointers[0]);
    }

    template <typename Component>
    const Component* entity_registry::try_get(entity e) const
    {
        constexpr type_id types[] = {get_type_id<Component>()};
        std::byte* pointers[1];

        find_component_data(e, types, pointers);

        return reinterpret_cast<const Component*>(pointers[0]);
    }

    template <typename Component>
    Component* entity_registry::try_get(entity e)
    {
        constexpr type_id types[] = {get_type_id<Component>()};
        std::byte* pointers[1];

        find_component_data(e, types, pointers);

        return reinterpret_cast<Component*>(pointers[0]);
    }

    template <typename... Components>
        requires(sizeof...(Components) > 1)
    std::tuple<const Components&...> entity_registry::get(entity e) const
    {
        constexpr auto N = sizeof...(Components);
        constexpr type_id types[] = {get_type_id<Components>()...};
        std::byte* pointers[N];

        find_component_data(e, types, pointers);

        constexpr auto makeTuple = []<std::size_t... I>(std::byte** pointers, std::index_sequence<I...>)
        {
            OBLO_ASSERT((pointers[I] && ...));
            return std::tuple<const Components&...>{*reinterpret_cast<Components*>(pointers[I])...};
        };

        return makeTuple(pointers, std::make_index_sequence<N>());
    }

    template <typename... Components>
        requires(sizeof...(Components) > 1)
    std::tuple<Components&...> entity_registry::get(entity e)
    {
        constexpr auto N = sizeof...(Components);
        constexpr type_id types[] = {get_type_id<Components>()...};
        std::byte* pointers[N];

        find_component_data(e, types, pointers);

        constexpr auto makeTuple = []<std::size_t... I>(std::byte** pointers, std::index_sequence<I...>)
        {
            OBLO_ASSERT((pointers[I] && ...));
            return std::tuple<Components&...>{*reinterpret_cast<Components*>(pointers[I])...};
        };

        return makeTuple(pointers, std::make_index_sequence<N>());
    }

    template <typename... ComponentsOrTags>
    decltype(auto) entity_registry::add(entity e)
    {
        const auto sets = make_type_sets<ComponentsOrTags...>(*m_typeRegistry);
        add(e, sets);
        return get<ComponentsOrTags...>(e);
    }

    template <typename... ComponentsOrTags>
    bool entity_registry::has(entity e) const
    {
        const auto querySets = make_type_sets<ComponentsOrTags...>(*m_typeRegistry);
        const auto entitySets = get_type_sets(e);

        return querySets.components.intersection(entitySets.components) == querySets.components &&
            querySets.tags.intersection(entitySets.tags) == querySets.tags;
    }
}