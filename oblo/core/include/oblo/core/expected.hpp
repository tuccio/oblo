#pragma once

#include <oblo/core/debug.hpp>
#include <oblo/core/types.hpp>

#include <type_traits>

namespace oblo
{
    // A tag that indicates failure, used as default error type for expected.
    struct unspecified_error
    {
    };

    template <typename T>
    concept trivial_type = std::is_trivial_v<T>;

    template <typename T>
    concept non_trivial_type = !std::is_trivial_v<T>;

    template <typename T, trivial_type E = unspecified_error>
    class [[nodiscard]] expected;

    // A simplified version of std::expected for trivial types.
    template <trivial_type T, trivial_type E>
    class [[nodiscard]] expected<T, E>
    {
    public:
        constexpr expected() = delete;
        constexpr expected(const expected&) = default;
        constexpr expected(expected&&) noexcept = default;
        constexpr expected(T value) : m_hasValue{true}, m_value{value} {}
        constexpr expected(E error) : m_hasValue{false}, m_error{error} {}

        constexpr expected& operator=(const expected&) = default;
        constexpr expected& operator=(expected&&) noexcept = default;

    public:
        constexpr const T* operator->() const noexcept
        {
            OBLO_ASSERT(has_value());
            return &m_value;
        }

        constexpr T* operator->() noexcept
        {
            OBLO_ASSERT(has_value());
            return &m_value;
        }

        constexpr const T& operator*() const noexcept
        {
            OBLO_ASSERT(has_value());
            return m_value;
        }

        constexpr T& operator*() noexcept
        {
            OBLO_ASSERT(has_value());
            return m_value;
        }

        constexpr bool has_value() const noexcept
        {
            return m_hasValue;
        }

        constexpr explicit operator bool() const noexcept
        {
            return m_hasValue;
        }

        constexpr E error() const noexcept
        {
            OBLO_ASSERT(!has_value());
            return m_error;
        }

        constexpr const T& value() const noexcept
        {
            OBLO_ASSERT(has_value());
            return m_value;
        }

        constexpr T value_or(const T& fallback) const noexcept
        {
            return m_hasValue ? m_value : fallback;
        }

    private:
        bool m_hasValue;
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
        constexpr expected() = delete;
        constexpr expected(const expected& other)
        {
            m_hasValue = other.m_hasValue;

            switch (other.m_hasValue)
            {
            case false:
                m_error = other.error;
                break;

            case true:
                new (m_value) T{*other};
                break;
            }
        }

        constexpr expected(expected&& other) noexcept
        {
            m_hasValue = other.m_hasValue;

            switch (other.m_hasValue)
            {
            case false:
                m_error = other.error;
                break;

            case true:
                new (m_value) T{std::move(*other)};
                break;
            }
        }

        template <typename U>
        constexpr expected(U&& value) : m_hasValue{true}
        {
            new (m_value) T{std::forward<U>(value)};
        }

        constexpr expected(E error) : m_hasValue{false}, m_error{error} {}

        constexpr ~expected()
        {
            if (m_hasValue)
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
            OBLO_ASSERT(has_value());
            return reinterpret_cast<const T*>(m_value);
        }

        constexpr T* operator->() noexcept
        {
            OBLO_ASSERT(has_value());
            return reinterpret_cast<T*>(m_value);
        }

        constexpr const T& operator*() const noexcept
        {
            OBLO_ASSERT(has_value());
            return *reinterpret_cast<const T*>(m_value);
        }

        constexpr T& operator*() noexcept
        {
            OBLO_ASSERT(has_value());
            return *reinterpret_cast<T*>(m_value);
        }

        constexpr bool has_value() const noexcept
        {
            return m_hasValue;
        }

        constexpr explicit operator bool() const noexcept
        {
            return m_hasValue;
        }

        constexpr E error() const noexcept
        {
            OBLO_ASSERT(!has_value());
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
            return m_hasValue ? *reinterpret_cast<T*>(m_value) : std::forward<U>(fallback);
        }

    private:
        bool m_hasValue;
        union {
            alignas(T) char m_value[sizeof(T)];
            E m_error;
        };
    };
}