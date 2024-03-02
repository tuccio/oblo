#pragma once

#include <oblo/core/debug.hpp>
#include <oblo/core/types.hpp>

#include <type_traits>

namespace oblo
{
    enum class expected_state : u8
    {
        uninitialized,
        error,
        valid,
    };

    // A monostate to indicate failure, used as default error enum for expected.
    enum class expected_monostate : u8
    {
        failure,
    };

    template <typename T>
    concept trivial_type = std::is_trivial_v<T>;

    template <typename T>
    concept non_trivial_type = !std::is_trivial_v<T>;

    template <typename T, trivial_type E = expected_monostate>
    class [[nodiscard]] expected;

    // A simplified version of std::expected for trivial types.
    template <trivial_type T, trivial_type E>
    class [[nodiscard]] expected<T, E>
    {
    public:
        constexpr expected() : m_state{expected_state::uninitialized} {}
        constexpr expected(const expected&) = default;
        constexpr expected(expected&&) noexcept = default;
        constexpr expected(T value) : m_state{expected_state::valid}, m_value{value} {}
        constexpr expected(E error) : m_state{expected_state::error}, m_error{error} {}

        constexpr expected& operator=(const expected&) = default;
        constexpr expected& operator=(expected&&) noexcept = default;

    public:
        constexpr const T* operator->() const noexcept
        {
            OBLO_ASSERT(m_state == expected_state::valid);
            return &m_value;
        }

        constexpr T* operator->() noexcept
        {
            OBLO_ASSERT(m_state == expected_state::valid);
            return &m_value;
        }

        constexpr const T& operator*() const noexcept
        {
            OBLO_ASSERT(m_state == expected_state::valid);
            return m_value;
        }

        constexpr T& operator*() noexcept
        {
            OBLO_ASSERT(m_state == expected_state::valid);
            return m_value;
        }

        constexpr bool has_value() const noexcept
        {
            return m_state == expected_state::valid;
        }

        constexpr explicit operator bool() const noexcept
        {
            return m_state == expected_state::valid;
        }

        constexpr E error() const noexcept
        {
            OBLO_ASSERT(m_state == expected_state::error);
            return m_error;
        }

        constexpr const T& value() const noexcept
        {
            OBLO_ASSERT(m_state == expected_state::valid);
            return m_value;
        }

        constexpr T value_or(const T& fallback) const noexcept
        {
            OBLO_ASSERT(m_state != expected_state::uninitialized);
            return m_state == expected_state::valid ? m_value : fallback;
        }

    private:
        expected_state m_state;
        union {
            T m_value;
            E m_error;
        };
    };

    // Implementation for non trivial types
    template <non_trivial_type T, trivial_type E>
    class [[nodiscard]] expected<T, E>
    {
    public:
        constexpr expected() : m_state{expected_state::uninitialized} {}
        constexpr expected(const expected& other)
        {
            m_state = other.m_state;

            switch (other.m_state)
            {
            case expected_state::uninitialized:
                break;

            case expected_state::error:
                m_error = other.error;
                break;

            case expected_state::valid:
                new (m_value) T{*other};
                break;
            }
        }

        constexpr expected(expected&& other) noexcept
        {
            m_state = other.m_state;

            switch (other.m_state)
            {
            case expected_state::uninitialized:
                break;

            case expected_state::error:
                m_error = other.error;
                break;

            case expected_state::valid:
                new (m_value) T{std::move(*other)};
                break;
            }
        }

        template <typename U>
        constexpr expected(U&& value) : m_state{expected_state::valid}
        {
            new (m_value) T{std::forward<U>(value)};
        }

        constexpr expected(E error) : m_state{expected_state::error}, m_error{error} {}

        constexpr ~expected()
        {
            if (m_state == expected_state::valid)
            {
                value().~T();
            }
        }

        constexpr expected& operator=(const expected& other)
        {
            this->~expected();
            new (this) expected{other};
            return *this;
        }

        constexpr expected& operator=(expected&& other) noexcept
        {
            this->~expected();
            new (this) expected{std::move(other)};
            return *this;
        }

    public:
        constexpr const T* operator->() const noexcept
        {
            OBLO_ASSERT(m_state == expected_state::valid);
            return reinterpret_cast<const T*>(m_value);
        }

        constexpr T* operator->() noexcept
        {
            OBLO_ASSERT(m_state == expected_state::valid);
            return reinterpret_cast<T*>(m_value);
        }

        constexpr const T& operator*() const noexcept
        {
            OBLO_ASSERT(m_state == expected_state::valid);
            return *reinterpret_cast<const T*>(m_value);
        }

        constexpr T& operator*() noexcept
        {
            OBLO_ASSERT(m_state == expected_state::valid);
            return *reinterpret_cast<T*>(m_value);
        }

        constexpr bool has_value() const noexcept
        {
            return m_state == expected_state::valid;
        }

        constexpr explicit operator bool() const noexcept
        {
            return m_state == expected_state::valid;
        }

        constexpr E error() const noexcept
        {
            OBLO_ASSERT(m_state == expected_state::error);
            return m_error;
        }

        constexpr T& value() noexcept
        {
            return **this;
        }

        constexpr const T& value() const noexcept
        {
            return **this;
        }

        template <typename U>
        constexpr T value_or(U&& fallback) const noexcept
        {
            OBLO_ASSERT(m_state != expected_state::uninitialized);
            return m_state == expected_state::valid ? *reinterpret_cast<T*>(m_value) : std::forward<U>(fallback);
        }

    private:
        expected_state m_state;
        union {
            alignas(T) char m_value[sizeof(T)];
            E m_error;
        };
    };
}