#pragma once

#include <oblo/core/zip_iterator.hpp>

namespace oblo
{
    template <typename Iterator>
    class iterator_range
    {
    public:
        iterator_range() = default;
        iterator_range(const iterator_range&) = default;
        iterator_range(iterator_range&&) noexcept = default;

        iterator_range(const Iterator& begin, const Iterator& end) : m_begin{begin}, m_end{end} {}

        iterator_range& operator=(const iterator_range&) = default;
        iterator_range& operator=(iterator_range&&) noexcept = default;

        auto begin() const
        {
            return m_begin;
        }

        auto end() const
        {
            return m_end;
        }

    private:
        Iterator m_begin;
        Iterator m_end;
    };

    template <typename Iterator>
    iterator_range(Iterator, Iterator) -> iterator_range<Iterator>;

    template <typename... Containers>
    auto zip_range(Containers&&... c)
    {
        return iterator_range{zip_iterator{std::begin(c)...}, zip_iterator{std::end(c)...}};
    }
}