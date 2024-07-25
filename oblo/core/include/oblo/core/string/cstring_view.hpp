#pragma once

#include <oblo/core/string/string_view.hpp>

namespace oblo
{
    class cstring_view : string_view
    {
    public:
        using string_view::const_iterator;
        using string_view::const_pointer;
        using string_view::const_reference;
        using string_view::const_reverse_iterator;
        using string_view::difference_type;
        using string_view::iterator;
        using string_view::pointer;
        using string_view::reference;
        using string_view::reverse_iterator;
        using string_view::size_type;
        using string_view::value_type;

        static constexpr size_type npos = ~size_type{};

    public:
        constexpr cstring_view() : string_view{""} {}
        constexpr cstring_view(const cstring_view&) = default;
        constexpr cstring_view(cstring_view&&) noexcept = default;

        constexpr cstring_view(const char* str) : string_view{str} {}
        constexpr cstring_view(const char8_t* str) : string_view{str} {}

        constexpr cstring_view(const char* str, usize length) : string_view{str, length}
        {
            OBLO_ASSERT(str[length] == '\0');
        }

        constexpr cstring_view(const char8_t* str, usize length) : string_view{str, length}
        {
            OBLO_ASSERT(str[length] == '\0');
        }

        constexpr cstring_view& operator=(const cstring_view&) = default;
        constexpr cstring_view& operator=(cstring_view&&) noexcept = default;

        const_pointer c_str() const noexcept
        {
            return data();
        }

        constexpr bool operator==(cstring_view str) const noexcept
        {
            return string_view{*this} == string_view{str};
        }

        int compare(cstring_view str) const noexcept
        {
            return string_view::compare(string_view{str});
        }

        using string_view::at;
        using string_view::back;
        using string_view::front;

        using string_view::data;
        using string_view::empty;
        using string_view::size;

        using string_view::swap;

        using string_view::copy;
        using string_view::ends_with;
        using string_view::starts_with;

        using string_view::remove_prefix;

        using string_view::begin;
        using string_view::end;

        using string_view::cbegin;
        using string_view::cend;

        using string_view::rbegin;
        using string_view::rend;

        using string_view::crbegin;
        using string_view::crend;

        using string_view::find;

        using string_view::as;

        constexpr operator string_view() const noexcept
        {
            return {data(), size()};
        }
    };

    inline namespace string_literals
    {
        constexpr string_view operator""_csv(const char* str, usize length)
        {
            return {str, length};
        }

        constexpr string_view operator""_csv(const char8_t* str, usize length)
        {
            return {str, length};
        }
    }
}