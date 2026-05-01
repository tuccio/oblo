#pragma once

#include <oblo/core/platform/compiler.hpp>
#include <oblo/core/types.hpp>

#include <iterator>
#include <tuple>
#include <type_traits>
#include <utility>

namespace oblo
{
    template <typename... Iterators>
    class concat_iterator
    {
    public:
        using iterator_tuple = std::tuple<Iterators...>;
        using iterator_category = std::forward_iterator_tag;

        using reference = std::common_reference_t<typename std::iterator_traits<Iterators>::reference...>;

        using value_type = std::remove_reference_t<reference>;
        using pointer = value_type*;
        using difference_type = ptrdiff;
        using size_type = usize;

        template <typename... B, typename... E>
        explicit concat_iterator(std::tuple<B...> begins, std::tuple<E...> ends, usize index = 0) :
            m_iterators{std::move(begins)}, m_ends{std::move(ends)}, m_index{index}
        {
            skip_empty();
        }

        concat_iterator() = default;
        concat_iterator(const concat_iterator&) = default;
        concat_iterator(concat_iterator&&) noexcept = default;

        concat_iterator& operator=(const concat_iterator&) = default;
        concat_iterator& operator=(concat_iterator&&) noexcept = default;

        OBLO_FORCEINLINE bool operator==(const concat_iterator& other) const
        {
            if (m_index != other.m_index)
            {
                return false;
            }

            if (m_index >= sizeof...(Iterators))
            {
                return true;
            }

            return equal_current(other, std::make_index_sequence<sizeof...(Iterators)>{});
        }

        OBLO_FORCEINLINE bool operator!=(const concat_iterator& other) const
        {
            return !(*this == other);
        }

        OBLO_FORCEINLINE concat_iterator& operator++()
        {
            increment(std::make_index_sequence<sizeof...(Iterators)>{});
            skip_empty();
            return *this;
        }

        OBLO_FORCEINLINE concat_iterator operator++(int)
        {
            const auto tmp = *this;
            ++*this;
            return tmp;
        }

        OBLO_FORCEINLINE reference operator*() const
        {
            return deref(std::make_index_sequence<sizeof...(Iterators)>{});
        }

    private:
        OBLO_FORCEINLINE void skip_empty()
        {
            while (m_index < sizeof...(Iterators) && at_end(std::make_index_sequence<sizeof...(Iterators)>{}))
            {
                ++m_index;
            }
        }

        template <usize... I>
        OBLO_FORCEINLINE bool at_end(std::index_sequence<I...>) const
        {
            bool result = false;
            ((m_index == I ? (result = (std::get<I>(m_iterators) == std::get<I>(m_ends)), void()) : void()), ...);
            return result;
        }

        template <usize... I>
        OBLO_FORCEINLINE void increment(std::index_sequence<I...>)
        {
            ((m_index == I ? (++std::get<I>(m_iterators), void()) : void()), ...);
        }

        template <usize... I>
        OBLO_FORCEINLINE reference deref(std::index_sequence<I...>) const
        {
            pointer result = nullptr;

            ((m_index == I ? (result = std::addressof(*std::get<I>(m_iterators)), void()) : void()), ...);

            return *result;
        }

        template <usize... I>
        OBLO_FORCEINLINE bool equal_current(const concat_iterator& other, std::index_sequence<I...>) const
        {
            bool result = false;

            ((m_index == I ? (result = (std::get<I>(m_iterators) == std::get<I>(other.m_iterators)), void()) : void()),
                ...);

            return result;
        }

    private:
        iterator_tuple m_iterators;
        iterator_tuple m_ends;
        usize m_index{0};
    };

    template <typename... Iterators>
    concat_iterator(std::tuple<Iterators...>, std::tuple<Iterators...>, usize) -> concat_iterator<Iterators...>;
}