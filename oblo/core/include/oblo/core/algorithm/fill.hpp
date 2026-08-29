#pragma once

#include <iterator>

namespace oblo
{
    template <typename Iterator, typename Value = typename std::iterator_traits<Iterator>::value_type>
    constexpr void fill(Iterator begin, const Iterator& end, const Value& v)
    {
        while (begin != end)
        {
            *begin = v;
            ++begin;
        }
    }
}