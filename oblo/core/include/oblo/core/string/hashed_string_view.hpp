#pragma once

#include <oblo/core/hash.hpp>
#include <oblo/core/string/string_view.hpp>

namespace oblo
{
    class hashed_string_view : string_view
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
        constexpr hashed_string_view() = default;

        constexpr hashed_string_view(string_view str, hash_type hash) : string_view{str}, m_hash{hash}
        {
            OBLO_ASSERT(hash_xxhz_compile_time(str.data(), str.size()) == hash);
        }

        constexpr hashed_string_view(const hashed_string_view&) = default;
        constexpr hashed_string_view(hashed_string_view&&) noexcept = default;

        OBLO_FORCEINLINE constexpr hashed_string_view(const char* str) : string_view{str} {}

        OBLO_FORCEINLINE constexpr hashed_string_view(const char8_t* str) : string_view{str} {}

        OBLO_FORCEINLINE constexpr hashed_string_view(const char* str, usize length) : string_view{str, length} {}

        OBLO_FORCEINLINE constexpr hashed_string_view(const char8_t* str, usize length) : string_view{str, length} {}

        template <contiguous_container_of<char> C>
        OBLO_FORCEINLINE constexpr explicit hashed_string_view(const C& c) : string_view{c.data(), c.size()}
        {
        }

        constexpr hashed_string_view& operator=(const hashed_string_view&) = default;
        constexpr hashed_string_view& operator=(hashed_string_view&&) noexcept = default;

        OBLO_FORCEINLINE constexpr bool operator==(hashed_string_view str) const noexcept
        {
            return m_hash == str.m_hash && string_view{*this} == string_view{str};
        }

        int compare(hashed_string_view str) const noexcept
        {
            return string_view::compare(string_view{str});
        }

        void swap(hashed_string_view& other)
        {
            auto h = m_hash;
            m_hash = other.m_hash;
            other.m_hash = h;

            string_view::swap(other);
        }

        OBLO_FORCEINLINE constexpr hash_type hash() const
        {
            return m_hash;
        }

        using string_view::at;
        using string_view::back;
        using string_view::front;

        using string_view::data;
        using string_view::empty;
        using string_view::size;
        using string_view::u8data;

        using string_view::swap;

        using string_view::copy;
        using string_view::ends_with;
        using string_view::starts_with;

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

    private:
        hash_type m_hash{hash_xxhz_compile_time(data(), size())};
    };

    inline namespace string_literals
    {
        consteval hashed_string_view operator""_hsv(const char* str, usize length)
        {
            return {str, length};
        }
    }

    template <>
    struct hash<hashed_string_view>
    {
        constexpr hash_type operator()(const hashed_string_view& sv) const noexcept
        {
            return sv.hash();
        }
    };
}