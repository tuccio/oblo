#pragma once

#include <oblo/core/iterator/zip_range.hpp>
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
        class chunk;

    public:
        typed_range& with(const component_and_tag_sets& sets);
        typed_range& exclude(const component_and_tag_sets& sets);

        template <typename... ComponentOrTags>
        typed_range& with();

        template <typename... ComponentOrTags>
        typed_range& exclude();

        typed_range& notified();

        typed_range& notified(u32 modificationId);

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
        bool m_onlyNotified = false;
        u32 m_modificationIdCheck;
        entity_registry* m_registry;
    };

    namespace detail
    {
        template <typename T>
        struct pointer_wrapper
        {
            T* pointer;
        };
    }

    template <typename... Components>
    class entity_registry::typed_range<Components...>::chunk :
        detail::pointer_wrapper<const entity>,
        detail::pointer_wrapper<Components>...
    {
    public:
        template <typename T>
        OBLO_FORCEINLINE auto get() const
        {
            using wrapper = detail::pointer_wrapper<T>;
            using mutable_wrapper = detail::pointer_wrapper<std::remove_const_t<T>>;
            using const_wrapper = detail::pointer_wrapper<const T>;

            if constexpr (std::is_base_of_v<wrapper, chunk>)
            {
                return std::span<T>{(static_cast<const wrapper*>(this))->pointer, m_numEntities};
            }
            else if constexpr (std::is_base_of_v<mutable_wrapper, chunk>)
            {
                return std::span<T>{(static_cast<const mutable_wrapper*>(this))->pointer, m_numEntities};
            }
            else if constexpr (std::is_base_of_v<const_wrapper, chunk>)
            {
                return std::span<const T>{(static_cast<const const_wrapper*>(this))->pointer, m_numEntities};
            }
        }

        template <typename... T>
        auto zip() const
        {
            return zip_range(get<T>()...);
        }

        void notify(bool notifyArchetype = false) const
        {
            const auto latestId = m_registry->get_modification_id();
            u32* const chunkModificationId = access_chunk_modification_id(m_archetype, m_chunkIndex);

            *chunkModificationId = latestId;

            // This is only here because we don't have a nice API to iterate archetypes yet
            if (notifyArchetype)
            {
                u32* const archetypeModificationId = access_archetype_modification_id(m_archetype);
                *archetypeModificationId = latestId;
            }
        }

    private:
        friend class entity_registry::typed_range<Components...>::iterator;

    private:
        const entity_registry* m_registry;
        archetype_storage m_archetype;
        u32 m_chunkIndex;
        u32 m_numEntities;
    };

    template <typename... Components>
    class entity_registry::typed_range<Components...>::iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = chunk;
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
            constexpr auto setPointers = []<std::size_t... I>(chunk& c,
                                             const entity* entities,
                                             std::byte** componentsData,
                                             const u8* mapping,
                                             std::index_sequence<I...>)
            {
                static_cast<detail::pointer_wrapper<const entity>&>(c).pointer = entities;

                ((static_cast<detail::pointer_wrapper<Components>&>(c).pointer =
                         reinterpret_cast<Components*>(componentsData[mapping[I]])),
                    ...);
            };

            byte* componentsData[sizeof...(Components)];
            const ecs::entity* entities;

            const u32 numEntities = fetch_chunk_data(*m_it, m_chunkIndex, m_offsets, &entities, componentsData);

            chunk c;
            c.m_archetype = *m_it;
            c.m_registry = m_range->m_registry;
            c.m_chunkIndex = m_chunkIndex;
            c.m_numEntities = numEntities;

            setPointers(c,
                entities,
                componentsData,
                m_range->m_mapping,
                std::make_index_sequence<sizeof...(Components)>());

            return c;
        }

        iterator& operator++()
        {
            auto* const registry = m_range->m_registry;
            auto* const modificationId = m_range->m_onlyNotified ? &m_range->m_modificationIdCheck : nullptr;

            do
            {
                if (++m_chunkIndex == m_numChunks)
                {
                    m_it = registry->find_first_match(m_it, 1, m_range->m_include, m_range->m_exclude, modificationId);

                    m_chunkIndex = 0;

                    if (!m_it || !update_iterator_data())
                    {
                        *this = {};
                        break;
                    }
                }

                if (!m_range->m_onlyNotified ||
                    *access_chunk_modification_id(*m_it, m_chunkIndex) >= m_range->m_modificationIdCheck)
                {
                    break;
                }

            } while (true);

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

        constexpr type_id types[] = {get_type_id<std::remove_const_t<Components>>()...};
        find_component_types(types, res.m_targets);

        u8 inverseMapping[numComponents];
        sort_and_map(res.m_targets, inverseMapping);

        for (u8 i = 0; i < numComponents; ++i)
        {
            res.m_mapping[inverseMapping[i]] = i;
        }

        res.m_include = make_type_sets<std::remove_const_t<Components>...>(*m_typeRegistry);
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
        return with(make_type_sets<std::remove_const_t<ComponentOrTags>...>(*m_registry->m_typeRegistry));
    }

    template <typename... Components>
    template <typename... ComponentOrTags>
    entity_registry::typed_range<Components...>& entity_registry::typed_range<Components...>::exclude()
    {
        return exclude(make_type_sets<std::remove_const_t<ComponentOrTags>...>(*m_registry->m_typeRegistry));
    }

    template <typename... Components>
    template <typename F>
    void entity_registry::typed_range<Components...>::for_each_chunk(F&& f) const
    {
        constexpr auto numComponents = sizeof...(Components);

        auto* const begin = m_registry->m_componentsStorage.data();
        auto* const modificationId = m_onlyNotified ? &m_modificationIdCheck : nullptr;

        for (auto* it = m_registry->find_first_match(begin, 0, m_include, m_exclude, modificationId); it != nullptr;
             it = m_registry->find_first_match(it, 1, m_include, m_exclude, modificationId))
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

                constexpr auto invoke = []<std::size_t... I>(F& f,
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

                invoke(f,
                    numEntities,
                    entities,
                    componentsData,
                    m_mapping,
                    std::make_index_sequence<sizeof...(Components)>());
            }
        }
    }

    template <typename... Components>
    entity_registry::typed_range<Components...>& entity_registry::typed_range<Components...>::notified()
    {
        return notified(m_registry->get_modification_id());
    }

    template <typename... Components>
    entity_registry::typed_range<Components...>& entity_registry::typed_range<Components...>::notified(
        u32 modificationId)
    {
        m_onlyNotified = true;
        m_modificationIdCheck = modificationId;
        return *this;
    }

    template <typename... Components>
    entity_registry::typed_range<Components...>::iterator entity_registry::typed_range<Components...>::begin() const
    {
        auto* const begin = m_registry->m_componentsStorage.data();
        auto* const modificationId = m_onlyNotified ? &m_modificationIdCheck : nullptr;
        return {this, m_registry->find_first_match(begin, 0, m_include, m_exclude, modificationId)};
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
            [&count](auto&& entities, auto&&...)
            {
                count += u32(entities.size());
                return count;
            });

        return count;
    }
}