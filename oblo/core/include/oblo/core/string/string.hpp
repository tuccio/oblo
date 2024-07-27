#pragma once

#include <bit>

#include <oblo/core/buffered_array.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/iterator/reverse_iterator.hpp>
#include <oblo/core/platform/compiler.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/string/string_view.hpp>
#include <oblo/core/types.hpp>

namespace oblo
{
    class string
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

        OBLO_FORCEINLINE string() : string{get_global_allocator()} {}

        OBLO_FORCEINLINE string(allocator* allocator) : m_string{allocator}
        {
            m_string.push_back('\0');
        }

        string(const string&) = default;
        string(string&&) noexcept = default;

        OBLO_FORCEINLINE string(const char* str) : string{str, std::strlen(str)} {}

        OBLO_FORCEINLINE string(const char8_t* str) : string{reinterpret_cast<const char*>(str)} {}

        OBLO_FORCEINLINE string(const char* str, size_type count)
        {
            m_string.assign(str, str + count);
            ensure_null_termination();
        }

        OBLO_FORCEINLINE string(const char8_t* str, size_type count) : string{reinterpret_cast<const char*>(str), count}
        {
        }

        OBLO_FORCEINLINE string(const char* begin, const char* end)
        {
            m_string.assign(begin, end);
            ensure_null_termination();
        }

        OBLO_FORCEINLINE string(const char8_t* begin, const char8_t* end) :
            string{reinterpret_cast<const char*>(begin), reinterpret_cast<const char*>(end)}
        {
        }

        string& operator=(const string&) = default;
        string& operator=(string&&) noexcept = default;

        OBLO_FORCEINLINE string& operator=(const string_view& sv)
        {
            m_string.assign(sv.begin(), sv.end());
            ensure_null_termination();
            return *this;
        }

        OBLO_FORCEINLINE const_pointer c_str() const noexcept
        {
            return m_string.data();
        }

        OBLO_FORCEINLINE const_pointer data() const noexcept
        {
            return m_string.data();
        }

        OBLO_FORCEINLINE size_type size() const noexcept
        {
            return m_string.size() - 1;
        }

        OBLO_FORCEINLINE bool empty() const noexcept
        {
            OBLO_ASSERT(!m_string.empty());
            return m_string.size() == 1;
        }

        OBLO_FORCEINLINE void reserve(usize size)
        {
            m_string.reserve(size);
        }

        OBLO_FORCEINLINE operator cstring_view() const noexcept
        {
            return {data(), size()};
        }

        template <typename T>
        OBLO_FORCEINLINE T as() const noexcept
        {
            return T{data(), size()};
        }

        OBLO_FORCEINLINE const_iterator begin() const
        {
            return m_string.cbegin();
        }

        OBLO_FORCEINLINE const_iterator end() const
        {
            return m_string.cbegin() + size();
        }

        OBLO_FORCEINLINE const_iterator cbegin() const
        {
            return m_string.cbegin();
        }

        OBLO_FORCEINLINE const_iterator cend() const
        {
            return m_string.cbegin() + size();
        }

        OBLO_FORCEINLINE const_reverse_iterator rbegin() const
        {
            return const_reverse_iterator{m_string.cbegin()};
        }

        OBLO_FORCEINLINE const_reverse_iterator rend() const
        {
            return const_reverse_iterator{m_string.cbegin() + size()};
        }

        OBLO_FORCEINLINE const_reverse_iterator crbegin() const
        {
            return const_reverse_iterator{m_string.cbegin()};
        }

        OBLO_FORCEINLINE const_reverse_iterator crend() const
        {
            return const_reverse_iterator{m_string.cbegin() + size()};
        }

        bool operator==(const string& other) const noexcept
        {
            return as<string_view>() == other.as<string_view>();
        }

    private:
        OBLO_FORCEINLINE void ensure_null_termination()
        {
            if (m_string.empty() || m_string.back() != '\0')
            {
                m_string.emplace_back('\0');
            }
        }

    private:
        buffered_array<char, 32> m_string;
    };
}