#pragma once

#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/type_set.hpp>

namespace oblo::ecs
{
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
        void for_each_chunk(F&& f) const;

    private:
        component_and_tags_sets m_include;
        component_and_tags_sets m_exclude;
        component_type m_targets[sizeof...(Components)];
        u8 m_mapping[sizeof...(Components)];
        entity_registry* m_registry;
    };

    template <typename... Components>
    entity_registry::typed_range<Components...> entity_registry::range()
    {
        typed_range<Components...> res;
        res.m_registry = this;

        constexpr type_id types[] = {get_type_id<Components>()...};
        find_component_types(types, res.m_targets);

        u8 inverseMapping[sizeof...(Components)];
        sort_and_map(res.m_targets, inverseMapping);

        for (u8 i = 0; i < sizeof...(Components); ++i)
        {
            res.m_mapping[inverseMapping[i]] = i;
        }

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
    void entity_registry::typed_range<Components...>::for_each_chunk(F&& f) const
    {
        constexpr auto numComponents = sizeof...(Components);

        const auto& components = m_include.components;

        auto* const begin = m_registry->m_componentsStorage.data();

        for (auto* it = m_registry->find_first_match(begin, 0, components); it != nullptr;
             it = m_registry->find_first_match(it, 1, components))
        {
            u32 offsets[numComponents];

            if (!fetch_component_offsets(*it, m_targets, offsets))
            {
                return;
            }

            const u32 numChunks = get_used_chunks_count(*it);

            for (u32 chunkIndex = 0; chunkIndex < numChunks; ++chunkIndex)
            {
                const entity* entities;
                std::byte* componentsData[numComponents];

                const u32 numEntities =
                    fetch_chunk_data(*it, chunkIndex, numChunks, offsets, &entities, componentsData);

                constexpr auto invoke = []<std::size_t... I>(F&& f,
                                                             u32 numEntities,
                                                             const entity* entities,
                                                             std::byte** componentsData,
                                                             const u8* mapping,
                                                             std::index_sequence<I...>)
                {
                    f(std::span{entities, numEntities},
                      std::span<Components>{reinterpret_cast<Components*>(componentsData[mapping[I]]), numEntities}...);
                };

                invoke(std::forward<F>(f),
                       numEntities,
                       entities,
                       componentsData,
                       m_mapping,
                       std::make_index_sequence<sizeof...(Components)>());
            }
        }
    }
}