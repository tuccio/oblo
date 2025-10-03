#pragma once

#include <oblo/core/platform/compiler.hpp>

#include <compare>
#include <concepts>
#include <iterator>
#include <type_traits>

namespace oblo
{
    template <class It>
    class reverse_iterator
    {
    public:
        reverse_iterator() = default;

        OBLO_FORCEINLINE constexpr explicit reverse_iterator(It itr) : m_current(std::move(itr)) {}

        template <class U>
            requires(!std::is_same_v<U, It> && std::convertible_to<const U&, It>)
        OBLO_FORCEINLINE constexpr explicit reverse_iterator(const U& other) : m_current(other.base())
        {
        }

        OBLO_FORCEINLINE constexpr decltype(auto) operator*() const
        {
            It prev = m_current;
            return *(--prev);
        }

        OBLO_FORCEINLINE constexpr decltype(auto) operator->() const
        {
            It prev = m_current;
            return &*(--prev);
        }

        OBLO_FORCEINLINE constexpr reverse_iterator& operator++()
        {
            --m_current;
            return *this;
        }

        OBLO_FORCEINLINE constexpr reverse_iterator operator++(int)
        {
            auto tmp = *this;
            ++(*this);
            return tmp;
        }

        OBLO_FORCEINLINE constexpr reverse_iterator& operator--()
        {
            ++m_current;
            return *this;
        }

        OBLO_FORCEINLINE constexpr reverse_iterator operator--(int)
        {
            auto tmp = *this;
            --(*this);
            return tmp;
        }

        OBLO_FORCEINLINE auto operator-(const reverse_iterator& other)
        {
            return other.m_current - m_current;
        }

        OBLO_FORCEINLINE constexpr It base() const
        {
            return m_current;
        }

        OBLO_FORCEINLINE constexpr auto operator<=>(const reverse_iterator& other) const = default;

    private:
        It m_current{};
    };

    template <typename T>
    OBLO_FORCEINLINE auto rbegin(T& c)
    {
        return reverse_iterator{std::end(c)};
    }

    template <typename T>
    OBLO_FORCEINLINE auto rend(T& c)
    {
        return reverse_iterator{std::begin(c)};
    }
}