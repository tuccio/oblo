#pragma once

#include <oblo/core/iterator/token_iterator.hpp>

namespace oblo
{
    class token_range
    {
    public:
        token_range() = default;
        token_range(const token_range&) = default;
        token_range(string_view str, string_view delim) : m_str{str}, m_delim{delim} {}
        token_range& operator=(const token_range&) = default;

        token_iterator begin() const
        {
            return {m_str, m_delim, 0};
        }

        token_iterator end() const
        {
            return {m_str, m_delim, string_view::npos};
        }

    private:
        string_view m_str;
        string_view m_delim;
    };
}