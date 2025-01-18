#pragma once

#include <oblo/core/buffered_array.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/string/format.hpp>
#include <oblo/core/string/string_view.hpp>

#include <format>
#include <span>

namespace oblo
{
    class string;

    class string_builder
    {
    public:
        string_builder();
        explicit string_builder(allocator* allocator);
        string_builder(allocator* allocator, string_view content);
        string_builder(const string_builder&) = default;
        string_builder(string_builder&&) noexcept = default;

        string_builder& operator=(const string_builder&) = default;
        string_builder& operator=(string_builder&&) noexcept = default;

        string_builder& append(char c);
        string_builder& append(const string& str);
        string_builder& append(cstring_view str);
        string_builder& append(string_view str);

        string_builder& append(const char* str, const char* end = nullptr);
        string_builder& append(const wchar_t* str, const wchar_t* end = nullptr);
        string_builder& append(const char8_t* str, const char8_t* end = nullptr);
        string_builder& append(const char16_t* str, const char16_t* end = nullptr);

        string_builder& append_path_separator();
        string_builder& append_path(string_view str);

        template <typename... Args>
        string_builder& format(std::format_string<Args...> fmt, Args&&... args);

        template <typename Iterator>
        string_builder& join(Iterator&& begin,
            Iterator&& end,
            string_view separator,
            std::format_string<decltype(*std::declval<Iterator>())> fmt = "{}");

        string_builder& make_absolute_path();
        string_builder& make_canonical_path();

        string_builder& trim_end();

        string_builder& clear();

        const char* c_str() const;
        const char* data() const;
        std::span<char> mutable_data();
        usize size() const;

        bool empty() const;

        const char* begin() const;

        const char* end() const;

        cstring_view view() const;

        explicit operator string_view() const;
        operator cstring_view() const;

        string_builder& operator=(string_view str);

        void reserve(usize size);
        void resize(usize size);

        template <typename T>
        T as() const noexcept;

        bool operator==(const string_builder& other) const noexcept;

    private:
        void ensure_null_termination();

    private:
        buffered_array<char, 256> m_buffer;
    };

    OBLO_FORCEINLINE string_builder::string_builder()
    {
        m_buffer.emplace_back('\0');
    }

    OBLO_FORCEINLINE string_builder::string_builder(allocator* allocator) : m_buffer{allocator}
    {
        m_buffer.emplace_back('\0');
    }

    OBLO_FORCEINLINE string_builder::string_builder(allocator* allocator, string_view content) : m_buffer{allocator}
    {
        m_buffer.insert(m_buffer.end(), content.begin(), content.end());
        m_buffer.emplace_back('\0');
    }

    OBLO_FORCEINLINE string_builder& string_builder::append(char c)
    {
        m_buffer.pop_back();
        m_buffer.push_back(c);
        ensure_null_termination();
        return *this;
    }

    OBLO_FORCEINLINE string_builder& string_builder::append(cstring_view str)
    {
        return append(string_view{str});
    }

    OBLO_FORCEINLINE string_builder& string_builder::append(string_view str)
    {
        m_buffer.pop_back();
        m_buffer.insert(m_buffer.end(), str.begin(), str.end());
        ensure_null_termination();
        return *this;
    }

    template <typename... Args>
    string_builder& string_builder::format(std::format_string<Args...> fmt, Args&&... args)
    {
        m_buffer.pop_back();
        std::format_to(std::back_inserter(m_buffer), fmt, std::forward<Args>(args)...);
        ensure_null_termination();
        return *this;
    }

    template <typename Iterator>
    string_builder& string_builder::join(Iterator&& begin,
        Iterator&& end,
        string_view separator,
        std::format_string<decltype(*std::declval<Iterator>())> fmt)
    {
        if (begin == end)
        {
            return *this;
        }

        format(fmt, *begin);

        for (auto it = std::next(begin); it != end; ++it)
        {
            append(separator);
            format(fmt, *it);
        }

        return *this;
    }

    template <typename T>
    OBLO_FORCEINLINE T string_builder::as() const noexcept
    {
        return T{data(), size()};
    }

    OBLO_FORCEINLINE string_builder& string_builder::clear()
    {
        m_buffer.assign(1u, '\0');
        return *this;
    }

    OBLO_FORCEINLINE const char* string_builder::c_str() const
    {
        return m_buffer.data();
    }

    OBLO_FORCEINLINE const char* string_builder::data() const
    {
        return m_buffer.data();
    }

    OBLO_FORCEINLINE std::span<char> string_builder::mutable_data()
    {
        return m_buffer;
    }

    OBLO_FORCEINLINE usize string_builder::size() const
    {
        OBLO_ASSERT(!m_buffer.empty());
        return m_buffer.size() - 1;
    }

    OBLO_FORCEINLINE bool string_builder::empty() const
    {
        OBLO_ASSERT(!m_buffer.empty());
        return m_buffer.size() == 1;
    }

    OBLO_FORCEINLINE const char* string_builder::begin() const
    {
        return data();
    }

    OBLO_FORCEINLINE const char* string_builder::end() const
    {
        return data() + size();
    }

    OBLO_FORCEINLINE void string_builder::ensure_null_termination()
    {
        if (m_buffer.empty() || m_buffer.back() != '\0')
        {
            m_buffer.emplace_back('\0');
        }
    }

    OBLO_FORCEINLINE cstring_view string_builder::view() const
    {
        return cstring_view{m_buffer.data(), size()};
    }

    OBLO_FORCEINLINE string_builder::operator string_view() const
    {
        return string_view{m_buffer.data(), size()};
    }

    OBLO_FORCEINLINE string_builder::operator cstring_view() const
    {
        return cstring_view{m_buffer.data(), size()};
    }

    OBLO_FORCEINLINE string_builder& string_builder::operator=(string_view str)
    {
        m_buffer.assign(str.begin(), str.end());
        ensure_null_termination();
        return *this;
    }

    OBLO_FORCEINLINE void string_builder::reserve(usize size)
    {
        m_buffer.reserve(size);
    }

    OBLO_FORCEINLINE void string_builder::resize(usize size)
    {
        m_buffer.reserve(size + 1);
        m_buffer.resize_default(size);
        ensure_null_termination();
    }
}

template <>
struct std::formatter<oblo::string_builder> : std::formatter<std::string_view>
{
    auto format(const oblo::string_builder& builder, std::format_context& ctx) const
    {
        const std::string_view sv{builder.data(), builder.size()};
        return std::formatter<std::string_view>::format(sv, ctx);
    }
};