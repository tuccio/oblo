#pragma once

#include <bit>

#include <oblo/core/concepts/sequential_container.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/iterator/reverse_iterator.hpp>
#include <oblo/core/platform/compiler.hpp>
#include <oblo/core/types.hpp>

namespace oblo
{
    class string_view
    {
    public:
        using value_type = char;
        using pointer = char*;
        using const_pointer = const char*;
        using reference = char&;
        using const_reference = const char&;
        using const_iterator = const char*;
        using iterator = const_iterator;
        using const_reverse_iterator = reverse_iterator<const_iterator>;
        using reverse_iterator = const_reverse_iterator;
        using size_type = usize;
        using difference_type = ptrdiff;

        static constexpr size_type npos = ~size_type{};

    public:
        constexpr string_view() = default;
        constexpr string_view(const string_view&) = default;
        constexpr string_view(string_view&&) noexcept = default;

        OBLO_FORCEINLINE constexpr string_view(const char8_t* str, size_type count) :
            string_view{std::bit_cast<const_pointer>(str), count}
        {
        }

        template <sequential_container_of<char> C>
        OBLO_FORCEINLINE constexpr string_view(const C& c) : m_begin{c.data()}, m_size{c.size()}
        {
        }

        OBLO_FORCEINLINE constexpr string_view(const char* str, size_type count) : m_begin{str}, m_size{count} {}

        OBLO_FORCEINLINE constexpr string_view(const char* str) : m_begin{str}, m_size{cstring_length(str)} {}
        constexpr string_view(const char8_t* str) : string_view{std::bit_cast<const_pointer>(str)} {}
        OBLO_FORCEINLINE
        constexpr string_view& operator=(const string_view&) = default;
        constexpr string_view& operator=(string_view&&) noexcept = default;

        OBLO_FORCEINLINE constexpr const_pointer data() const noexcept
        {
            return m_begin;
        }

        constexpr const_reference at(usize i) const noexcept
        {
            OBLO_ASSERT(i < m_size);
            return m_begin[i];
        }

        constexpr char front() const noexcept
        {
            OBLO_ASSERT(m_size > 0);
            return m_begin[0];
        }

        constexpr char back() const noexcept
        {
            OBLO_ASSERT(m_size > 0);
            return m_begin[m_size - 1];
        }

        OBLO_FORCEINLINE constexpr usize size() const noexcept
        {
            return m_size;
        }

        OBLO_FORCEINLINE constexpr bool empty() const noexcept
        {
            return m_size == 0;
        }

        constexpr string_view substr(size_type pos = 0, size_type count = npos) const noexcept
        {
            OBLO_ASSERT(pos <= m_size);
            return {m_begin + pos, clamp_offset(pos, count)};
        }

        constexpr void remove_prefix(usize n)
        {
            *this = substr(n);
        }

        constexpr void remove_suffix(usize n)
        {
            *this = substr(0, m_size - n);
        }

        constexpr void swap(string_view& other)
        {
            const auto tmp = *this;
            *this = other;
            other = tmp;
        }

        OBLO_FORCEINLINE constexpr bool starts_with(string_view str) const
        {
            return substr(0, str.size()) == *this;
        }

        OBLO_FORCEINLINE constexpr bool ends_with(string_view str) const
        {
            return substr(0, str.size()) == *this;
        }

        constexpr size_type copy(pointer dst, size_type count, size_type pos = 0) const noexcept
        {
            OBLO_ASSERT(pos <= m_size);

            const auto rcount = clamp_offset(pos, count);

            for (const_pointer b = m_begin, e = m_begin + rcount; b != e; ++b)
            {
                *dst = *b;
                ++dst;
            }

            return rcount;
        }

        int compare(string_view str) const noexcept
        {
            const auto rlen = m_size < str.m_size ? m_size : str.m_size;
            const auto r = std::memcmp(m_begin, str.m_begin, rlen);

            if (r < 0 || (r == 0 && m_size < str.m_size))
            {
                return -1;
            }
            else if (r > 0 || (r == 0 && m_size > str.m_size))
            {
                return 1;
            }
            else
            {
                return 0;
            }
        }

        OBLO_FORCEINLINE constexpr bool operator==(string_view str) const noexcept
        {
            if (m_size != str.m_size)
            {
                return false;
            }

            for (size_type i = 0; i < m_size; ++i)
            {
                if (m_begin[i] != str.m_begin[i])
                {
                    return false;
                }
            }

            return true;
        }

        constexpr size_type find(string_view v, size_type pos = 0) const noexcept
        {
            if (v.size() > m_size)
            {
                return npos;
            }

            const auto last = 1 + m_size - v.size();

            for (size_type i = pos; i < last; ++i)
            {
                if (const auto candidate = substr(i, v.size()); candidate == v)
                {
                    return i;
                }
            }

            return npos;
        }

        constexpr size_type find(value_type ch, size_type pos = 0) const noexcept
        {
            for (size_type i = pos; i < m_size; ++i)
            {
                if (at(i) == ch)
                {
                    return i;
                }
            }

            return npos;
        }

        OBLO_FORCEINLINE constexpr const_iterator begin() const
        {
            return m_begin;
        }

        OBLO_FORCEINLINE constexpr const_iterator end() const
        {
            return m_begin + m_size;
        }

        OBLO_FORCEINLINE constexpr const_iterator cbegin() const
        {
            return m_begin;
        }

        OBLO_FORCEINLINE constexpr const_iterator cend() const
        {
            return m_begin + m_size;
        }

        OBLO_FORCEINLINE constexpr const_reverse_iterator rbegin() const
        {
            return const_reverse_iterator{m_begin};
        }

        OBLO_FORCEINLINE constexpr const_reverse_iterator rend() const
        {
            return const_reverse_iterator{m_begin + m_size};
        }

        OBLO_FORCEINLINE constexpr const_reverse_iterator crbegin() const
        {
            return const_reverse_iterator{m_begin};
        }

        OBLO_FORCEINLINE constexpr const_reverse_iterator crend() const
        {
            return const_reverse_iterator{m_begin + m_size};
        }

        template <typename T>
        T as() const noexcept
        {
            return T{m_begin, m_size};
        }

    private:
        constexpr size_type clamp_offset(size_type offset, size_type count) const
        {
            const auto r = m_size - offset;
            return r < count ? r : count;
        }

        static constexpr usize cstring_length(const char* ptr)
        {
            usize i = 0;

            while (ptr[i] != '\0')
            {
                ++i;
            }

            return i;
        }

    private:
        const char* m_begin{};
        size_type m_size{};
    };

    inline namespace string_literals
    {
        consteval string_view operator""_sv(const char* str, usize length)
        {
            return {str, length};
        }

        consteval string_view operator""_sv(const char8_t* str, usize length)
        {
            return {str, length};
        }
    }
}