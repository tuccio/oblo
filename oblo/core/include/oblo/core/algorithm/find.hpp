#pragma once

#include <iterator>

namespace oblo
{
    template <typename Iterator, typename Value = typename std::iterator_traits<Iterator>::value_type>
    constexpr Iterator find(Iterator begin, const Iterator& end, const Value& v)
    {
        while (begin != end)
        {
            if (*begin == v)
            {
                break;
            }

            ++begin;
        }

        return begin;
    }

    template <typename Iterator, typename Predicate>
    constexpr Iterator find_if(Iterator begin, const Iterator& end, const Predicate& p)
    {
        while (begin != end)
        {
            if (p(*begin))
            {
                break;
            }

            ++begin;
        }

        return begin;
    }
}