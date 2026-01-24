#pragma once

#include <oblo/core/string/string_view.hpp>
#include <oblo/core/types.hpp>

#include <iterator>

namespace oblo
{
    class token_iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = string_view;
        using difference_type = ptrdiff;
        using pointer = const string_view*;
        using reference = string_view;

        token_iterator() = default;

        token_iterator(string_view str, string_view delim, usize pos) : m_str(str), m_delim(delim), m_pos(pos)
        {
            advance();
        }

        reference operator*() const
        {
            return m_current;
        }

        token_iterator& operator++()
        {
            m_pos = m_next;
            advance();
            return *this;
        }

        token_iterator operator++(int)
        {
            token_iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        friend bool operator==(const token_iterator& a, const token_iterator& b)
        {
            return a.m_pos == b.m_pos && a.m_str.data() == b.m_str.data();
        }

        friend bool operator!=(const token_iterator& a, const token_iterator& b)
        {
            return !(a == b);
        }

    private:
        string_view m_str;
        string_view m_delim;
        usize m_pos = 0;
        usize m_next = string_view::npos;
        string_view m_current{};

        void advance()
        {
            if (m_pos == string_view::npos)
                return;

            if (m_pos > m_str.size())
            {
                m_pos = string_view::npos;
                return;
            }

            auto found = m_str.find(m_delim, m_pos);
            if (found == string_view::npos)
            {
                m_current = m_str.substr(m_pos);
                m_pos = m_str.size() + 1; // marks end
                m_next = string_view::npos;
            }
            else
            {
                m_current = m_str.substr(m_pos, found - m_pos);
                m_next = found + m_delim.size();
            }
        }
    };
}