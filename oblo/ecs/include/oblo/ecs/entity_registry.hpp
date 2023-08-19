#pragma once

#include <oblo/core/flat_dense_map.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/ecs/handles.hpp>
#include <oblo/ecs/traits.hpp>
#include <oblo/ecs/type_set.hpp>

#include <memory>

namespace oblo::ecs
{
    class type_registry;

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

        void clear();

        template <typename... Components>
        typed_range<Components...> range();

    private:
        struct components_storage;
        struct tags_storage;
        struct entity_data;
        struct memory_pool;

    private:
        const components_storage& find_or_create_component_storage(const type_set& components);

        const components_storage* find_first_match(const components_storage* begin, const type_set& components);

        void find_and_sort_component_types(std::span<const type_id> typeIds, std::span<component_type> types);

    private:
        const type_registry* m_typeRegistry{nullptr};
        flat_dense_map<entity, entity_data> m_entities;
        std::vector<components_storage> m_componentsStorage;
        std::vector<tags_storage> m_tagsStorage;
        std::unique_ptr<memory_pool> m_pool;
        entity m_nextId{1};
    };

    template <typename... Components>
    class entity_registry::typed_range
    {
        friend class entity_registry;

    public:
        template <typename... ComponentOrTags>
        typed_range& with();

        template <typename... ComponentOrTags>
        typed_range& exclude();

        template <typename F>
        void for_each_chunk(F&& f);

    private:
        component_and_tags_sets m_include;
        component_and_tags_sets m_exclude;
        component_type m_targets[sizeof...(Components)];
        entity_registry* m_registry;
    };

    template <typename... ComponentsOrTags>
    entity entity_registry::create(u32 count)
    {
        return create(make_type_sets<ComponentsOrTags...>(*m_typeRegistry), count);
    }

    template <typename... Components>
    entity_registry::typed_range<Components...> entity_registry::range()
    {
        typed_range<Components...> res;
        res.m_registry = this;

        constexpr type_id types[] = {get_type_id<Components>()...};
        find_and_sort_component_types(types, res.m_targets);

        res.m_include = make_type_sets<Components...>(*m_typeRegistry);
        return res;
    }

    template <typename... Components>
    template <typename... ComponentOrTags>
    entity_registry::typed_range<Components...>& entity_registry::typed_range<Components...>::with()
    {
        const auto includes = make_type_sets<Components...>(*m_typeRegistry);

        m_include.components.add(includes.components);
        m_include.tags.add(includes.tags);

        return *this;
    }

    template <typename... Components>
    template <typename... ComponentOrTags>
    entity_registry::typed_range<Components...>& entity_registry::typed_range<Components...>::exclude()
    {
        const auto excludes = make_type_sets<Components...>(*m_typeRegistry);

        m_excludes.components.add(includes.components);
        m_excludes.tags.add(includes.tags);

        return *this;
    }

    template <typename... Components>
    template <typename F>
    void entity_registry::typed_range<Components...>::for_each_chunk(F&& f)
    {
        // TODO
        (void) f;
    }
}