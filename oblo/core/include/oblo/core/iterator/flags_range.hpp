#pragma once

#include <oblo/core/flags.hpp>
#include <oblo/core/platform/compiler.hpp>

#include <bit>
#include <type_traits>

namespace oblo
{
    template <typename T, typename U>
    class flags_iterator
    {
    public:
        constexpr flags_iterator() = default;
        constexpr flags_iterator(const flags_iterator&) = default;
        constexpr flags_iterator(flags_iterator&&) noexcept = default;

        constexpr explicit flags_iterator(U value) : m_value{value} {}

        constexpr flags_iterator& operator=(const flags_iterator&) = default;
        constexpr flags_iterator& operator=(flags_iterator&&) noexcept = default;

        constexpr bool operator==(const flags_iterator& other) const = default;

        OBLO_FORCEINLINE flags_iterator& operator++()
        {
            const auto n = std::countr_zero(m_value);
            const U mask = (U{1} << (n + 1)) - 1;
            m_value &= ~mask;
            return *this;
        }

        OBLO_FORCEINLINE flags_iterator operator++(int)
        {
            const auto it = *this;
            ++*this;
            return it;
        }

        OBLO_FORCEINLINE T operator*() const
        {
            const auto n = std::countr_zero(m_value);
            return T(n);
        }

    private:
        U m_value{};
    };

    template <typename T, u32 Size>
        requires std::is_enum_v<T>
    class flags_range
    {
    public:
        using iterator = flags_iterator<T, decltype(flags<T, Size>{}.data())>;

    public:
        flags_range() = default;
        flags_range(const flags_range&) = default;
        flags_range(flags_range&&) noexcept = default;

        explicit constexpr flags_range(flags<T, Size> flags) : m_begin{flags.data()}, m_end{} {}

        flags_range& operator=(const flags_range&) = default;
        flags_range& operator=(flags_range&&) noexcept = default;

        constexpr iterator begin() const
        {
            return m_begin;
        }

        constexpr iterator end() const
        {
            return m_end;
        }

    private:
        iterator m_begin{};
        iterator m_end{};
    };
}