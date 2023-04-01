#pragma once

#include <oblo/core/types.hpp>

#include <concepts>

namespace oblo
{
    // A simplified version of std::expected for trivial types.
    template <typename T, typename E>
    requires std::is_trivial_v<T> && std::is_trivial_v<E>
    class expected
    {
        enum class state : u8
        {
            uninitialized,
            error,
            valid,
        };

    public:
        expected() : m_state{state::uninitialized} {}
        expected(const expected&) = default;
        expected(expected&&) noexcept = default;
        expected(T value) : m_state{state::valid}, m_value{value} {}
        expected(E error) : m_state{state::error}, m_error{error} {}

        expected& operator=(const expected&) = default;
        expected& operator=(expected&&) noexcept = default;

    public:
        constexpr const T* operator->() const noexcept
        {
            OBLO_ASSERT(m_state == state::valid);
            return &m_value;
        }

        constexpr T* operator->() noexcept
        {
            OBLO_ASSERT(m_state == state::valid);
            return &m_value;
        }

        constexpr const T& operator*() const noexcept
        {
            OBLO_ASSERT(m_state == state::valid);
            return m_value;
        }

        constexpr T& operator*() noexcept
        {
            OBLO_ASSERT(m_state == state::valid);
            return m_value;
        }

        constexpr bool has_value() const noexcept
        {
            return m_state == state::valid;
        }

        constexpr explicit operator bool() const noexcept
        {
            return m_state == state::valid;
        }

        constexpr E error() const noexcept
        {
            OBLO_ASSERT(m_state == state::error);
            return m_error;
        }

        constexpr E value() const noexcept
        {
            OBLO_ASSERT(m_state == state::valid);
            return m_value;
        }

        constexpr T value_or(const T& fallback) const noexcept
        {
            OBLO_ASSERT(m_state != state::uninitialized);
            return m_state == state::valid ? m_value : fallback;
        }

    private:
        state m_state;
        union {
            T m_value;
            E m_error;
        };
    };
}