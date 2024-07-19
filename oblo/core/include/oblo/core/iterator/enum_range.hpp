#pragma once

#include <oblo/core/platform/compiler.hpp>

#include <type_traits>

namespace oblo
{
    template <typename T>
    class enum_iterator
    {
    public:
        constexpr enum_iterator() = default;
        constexpr enum_iterator(const enum_iterator&) = default;
        constexpr enum_iterator(enum_iterator&&) noexcept = default;

        constexpr explicit enum_iterator(T value) : m_value{value} {}

        constexpr enum_iterator& operator=(const enum_iterator&) = default;
        constexpr enum_iterator& operator=(enum_iterator&&) noexcept = default;

        constexpr bool operator==(const enum_iterator& other) const = default;

        OBLO_FORCEINLINE enum_iterator& operator++()
        {
            m_value = T(U(m_value) + 1);
            return *this;
        }

        OBLO_FORCEINLINE enum_iterator operator++(int)
        {
            const auto it = *this;
            ++*this;
            return it;
        }

        OBLO_FORCEINLINE enum_iterator& operator--()
        {
            m_value = T(U(m_value) - 1);
            return *this;
        }

        OBLO_FORCEINLINE enum_iterator operator--(int)
        {
            const auto it = *this;
            --*this;
            return it;
        }

        OBLO_FORCEINLINE T operator*() const
        {
            return m_value;
        }

    private:
        using U = std::underlying_type_t<T>;

    private:
        T m_value;
    };

    template <typename T>
        requires std::is_enum_v<T>
    class enum_range
    {
    public:
        enum_range() = default;
        enum_range(const enum_range&) = default;
        enum_range(enum_range&&) noexcept = default;

        explicit enum_range(T begin, T end = T::enum_max) : m_begin{begin}, m_end{end} {}

        explicit enum_range(enum_iterator<T> begin, enum_iterator<T> end = enum_iterator<T>{T::enum_max}) :
            m_begin{begin}, m_end{end}
        {
        }

        enum_range& operator=(const enum_range&) = default;
        enum_range& operator=(enum_range&&) noexcept = default;

        constexpr enum_iterator<T> begin() const
        {
            return m_begin;
        }

        constexpr enum_iterator<T> end() const
        {
            return m_end;
        }

    private:
        enum_iterator<T> m_begin{};
        enum_iterator<T> m_end{T::enum_max};
    };
}