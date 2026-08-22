#pragma once

namespace oblo
{
    template <typename Iterator, typename T>
    Iterator find(Iterator begin, const Iterator& end, const T& v)
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
    Iterator find_if(Iterator begin, const Iterator& end, Predicate&& p)
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

    template <typename Iterator, typename Predicate>
    Iterator find_if_not(Iterator begin, const Iterator& end, Predicate&& p)
    {
        while (begin != end)
        {
            if (!p(*begin))
            {
                break;
            }

            ++begin;
        }

        return begin;
    }
}