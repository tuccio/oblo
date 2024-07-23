#pragma once

#include <oblo/core/buffered_array.hpp>

#include <format>

namespace oblo
{
    class string_builder
    {
    public:
        string_builder();
        explicit string_builder(allocator* allocator);
        string_builder(const string_builder&) = delete;
        string_builder(string_builder&&) noexcept = delete;

        string_builder& operator=(const string_builder&) = delete;
        string_builder& operator=(string_builder&&) noexcept = delete;

        void append(std::string_view str);

        template <typename... Args>
        void append(std::format_string<Args...>, Args&&... args);

        void set(std::string_view str);

        template <typename... Args>
        void set(std::format_string<Args...>, Args&&... args);

        void clear();

        const char* data() const;
        usize size() const;

        std::string_view view() const;

        explicit operator std::string_view() const;

        string_builder& operator=(std::string_view str);

    private:
        void ensure_null_termination();

    private:
        buffered_array<char, 256> m_buffer;
    };

    inline string_builder::string_builder()
    {
        m_buffer.emplace_back('\0');
    }

    inline string_builder::string_builder(allocator* allocator) : m_buffer{allocator}
    {
        m_buffer.emplace_back('\0');
    }

    inline void string_builder::append(std::string_view str)
    {
        m_buffer.pop_back();
        m_buffer.insert(m_buffer.end(), str.begin(), str.end());
        ensure_null_termination();
    }

    template <typename... Args>
    void string_builder::append(std::format_string<Args...> fmt, Args&&... args)
    {
        m_buffer.pop_back();
        const auto outIt = std::format_to(std::back_inserter(m_buffer), fmt, std::forward<Args>(args)...);
        ensure_null_termination();
    }

    inline void string_builder::set(std::string_view str)
    {
        m_buffer.assign(str.begin(), str.end());
        ensure_null_termination();
    }

    template <typename... Args>
    void string_builder::set(std::format_string<Args...> fmt, Args&&... args)
    {
        m_buffer.clear();
        const auto outIt = std::format_to(std::back_inserter(m_buffer), fmt, std::forward<Args>(args)...);
        ensure_null_termination();
    }

    inline void string_builder::clear()
    {
        m_buffer.assign(1u, '\0');
    }

    inline const char* string_builder::data() const
    {
        return m_buffer.data();
    }

    inline usize string_builder::size() const
    {
        OBLO_ASSERT(!m_buffer.empty());
        return m_buffer.size() - 1;
    }

    inline void string_builder::ensure_null_termination()
    {
        OBLO_ASSERT(!m_buffer.empty());

        if (m_buffer.back() != '\0')
        {
            m_buffer.emplace_back('\0');
        }
    }

    inline std::string_view string_builder::view() const
    {
        return std::string_view{m_buffer.data(), size()};
    }

    inline string_builder::operator std::string_view() const
    {
        return std::string_view{m_buffer.data(), size()};
    }

    inline string_builder& string_builder::operator=(std::string_view str)
    {
        m_buffer.assign(str.begin(), str.end());
        ensure_null_termination();
    }
}