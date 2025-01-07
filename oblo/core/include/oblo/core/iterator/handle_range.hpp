#pragma once

#include <iterator>

namespace oblo
{
    template <typename Handle>
    class handle_range
    {
    public:
        class iterator
        {
        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = Handle;
            using difference_type = std::ptrdiff_t;
            using pointer = Handle*;
            using reference = Handle&;

            iterator(Handle current) : m_current{current} {}

            Handle operator*() const
            {
                return m_current;
            }

            iterator& operator++()
            {
                ++m_current.value;
                return *this;
            }

            iterator operator++(int)
            {
                iterator temp = *this;
                ++(*this);
                return temp;
            }

            bool operator==(const iterator& other) const
            {
                return m_current.value == other.m_current.value;
            }

            bool operator!=(const iterator& other) const
            {
                return !(*this == other);
            }

        private:
            Handle m_current;
        };

        handle_range(Handle start, Handle end) : m_start{start}, m_end{end} {}

        iterator begin() const
        {
            return iterator(m_start);
        }

        iterator end() const
        {
            return iterator(m_end);
        }

    private:
        Handle m_start;
        Handle m_end;
    };
}