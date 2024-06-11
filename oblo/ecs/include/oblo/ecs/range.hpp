#pragma once

#include <oblo/ecs/archetype_storage.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/type_set.hpp>

#include <iterator>
#include <tuple>

namespace oblo::ecs
{
    template <typename... Components>
    class entity_registry::typed_range
    {
        friend class entity_registry;

    public:
        class iterator;

    public:
        typed_range& with(const component_and_tag_sets& sets);
        typed_range& exclude(const component_and_tag_sets& sets);

        template <typename... ComponentOrTags>
        typed_range& with();

        template <typename... ComponentOrTags>
        typed_range& exclude();

        template <typename F>
        void for_each_chunk(F&& f) const;

        iterator begin() const;

        iterator end() const;

        u32 count() const;

    private:
        component_and_tag_sets m_include;
        component_and_tag_sets m_exclude;
        component_type m_targets[sizeof...(Components)];
        u8 m_mapping[sizeof...(Components)];
        entity_registry* m_registry;
    };

    template <typename... Components>
    class entity_registry::typed_range<Components...>::iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::tuple<std::span<const entity>, std::span<Components>...>;
        using reference = value_type;
        using pointer = value_type*;
        using difference_type = std::ptrdiff_t;
        using size_type = std::size_t;

    public:
        iterator() = default;
        iterator(const iterator&) = default;
        iterator& operator=(const iterator&) = default;

        reference operator*() const
        {
            const entity* entities;
            std::byte* componentsData[sizeof...(Components)];

            const u32 numEntities = fetch_chunk_data(*m_it, m_chunkIndex, m_offsets, &entities, componentsData);

            constexpr auto makeTuple = []<std::size_t... I>(u32 numEntities,
                                           const entity* entities,
                                           std::byte** componentsData,
                                           const u8* mapping,
                                           std::index_sequence<I...>)
            {
                return value_type{std::span{entities, numEntities},
                    std::span<Components>{reinterpret_cast<Components*>(componentsData[mapping[I]]), numEntities}...};
            };

            return makeTuple(numEntities,
                entities,
                componentsData,
                m_range->m_mapping,
                std::make_index_sequence<sizeof...(Components)>());
        }

        iterator& operator++()
        {
            if (++m_chunkIndex == m_numChunks)
            {
                m_it = m_range->m_registry->find_first_match(m_it, 1, m_range->m_include, m_range->m_exclude);
                m_chunkIndex = 0;

                if (!m_it || !update_iterator_data())
                {
                    *this = {};
                }
            }

            return *this;
        }

        iterator operator++(int)
        {
            iterator it = *this;
            ++(*this);
            return it;
        }

        friend bool operator==(const iterator& lhs, const iterator& rhs)
        {
            return lhs.m_it == rhs.m_it && lhs.m_chunkIndex == rhs.m_chunkIndex;
        };

        friend bool operator!=(const iterator& lhs, const iterator& rhs)
        {
            return !(lhs == rhs);
        };

    private:
        using range_type = typed_range<Components...>;
        friend class typed_range<Components...>;

    private:
        iterator(const range_type* range, const archetype_storage* it) : m_range{range}, m_it{it}
        {
            if (!update_iterator_data())
            {
                *this = {};
            }
        };

        bool update_iterator_data()
        {
            if (!m_it || !fetch_component_offsets(*m_it, m_range->m_targets, m_offsets))
            {
                return false;
            }

            m_numChunks = get_used_chunks_count(*m_it);
            return true;
        }

    private:
        const range_type* m_range{nullptr};
        const archetype_storage* m_it{nullptr};
        u32 m_chunkIndex{0};
        u32 m_numChunks{0};
        u32 m_offsets[sizeof...(Components)];
    };

    template <typename... Components>
    entity_registry::typed_range<Components...> entity_registry::range()
    {
        constexpr u8 numComponents = sizeof...(Components);

        typed_range<Components...> res;
        res.m_registry = this;

        constexpr type_id types[] = {get_type_id<Components>()...};
        find_component_types(types, res.m_targets);

        u8 inverseMapping[numComponents];
        sort_and_map(res.m_targets, inverseMapping);

        for (u8 i = 0; i < numComponents; ++i)
        {
            res.m_mapping[inverseMapping[i]] = i;
        }

        res.m_include = make_type_sets<Components...>(*m_typeRegistry);
        res.m_exclude = {};

        return res;
    }

    template <typename... Components>
    entity_registry::typed_range<Components...>& entity_registry::typed_range<Components...>::with(
        const component_and_tag_sets& includes)
    {
        m_include.components.add(includes.components);
        m_include.tags.add(includes.tags);

        return *this;
    }

    template <typename... Components>
    entity_registry::typed_range<Components...>& entity_registry::typed_range<Components...>::exclude(
        const component_and_tag_sets& excludes)
    {
        m_exclude.components.add(excludes.components);
        m_exclude.tags.add(excludes.tags);

        return *this;
    }

    template <typename... Components>
    template <typename... ComponentOrTags>
    entity_registry::typed_range<Components...>& entity_registry::typed_range<Components...>::with()
    {
        return with(make_type_sets<ComponentOrTags...>(*m_registry->m_typeRegistry));
    }

    template <typename... Components>
    template <typename... ComponentOrTags>
    entity_registry::typed_range<Components...>& entity_registry::typed_range<Components...>::exclude()
    {
        return exclude(make_type_sets<ComponentOrTags...>(*m_registry->m_typeRegistry));
    }

    template <typename... Components>
    template <typename F>
    void entity_registry::typed_range<Components...>::for_each_chunk(F&& f) const
    {
        constexpr auto numComponents = sizeof...(Components);

        auto* const begin = m_registry->m_componentsStorage.data();

        for (auto* it = m_registry->find_first_match(begin, 0, m_include, m_exclude); it != nullptr;
             it = m_registry->find_first_match(it, 1, m_include, m_exclude))
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

                const u32 numEntities = fetch_chunk_data(*it, chunkIndex, offsets, &entities, componentsData);

                OBLO_ASSERT(numEntities != 0);

                constexpr auto invoke = []<std::size_t... I>(F&& f,
                                            u32 numEntities,
                                            const entity* entities,
                                            std::byte** componentsData,
                                            const u8* mapping,
                                            std::index_sequence<I...>)
                {
                    f(std::span{entities, numEntities},
                        std::span<Components>{reinterpret_cast<Components*>(componentsData[mapping[I]]),
                            numEntities}...);
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

    template <typename... Components>
    entity_registry::typed_range<Components...>::iterator entity_registry::typed_range<Components...>::begin() const
    {
        auto* const begin = m_registry->m_componentsStorage.data();
        return {this, m_registry->find_first_match(begin, 0, m_include, m_exclude)};
    }

    template <typename... Components>
    entity_registry::typed_range<Components...>::iterator entity_registry::typed_range<Components...>::end() const
    {
        return {};
    }

    template <typename... Components>
    u32 entity_registry::typed_range<Components...>::count() const
    {
        u32 count{};

        for_each_chunk(
            [&count](auto&&... spans)
            {
                count += (u32(spans.size()) + ...);
                return count;
            });

        return count;
    }
}