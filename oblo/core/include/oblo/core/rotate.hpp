#pragma once

#include <iterator>
#include <utility>

namespace oblo
{
    // Simple implementation of std::rotate for forward iterators, random access could be smarter.
    template <std::forward_iterator Iterator>
    Iterator rotate(Iterator first, Iterator middle, Iterator last)
    {
        Iterator it = middle;

        while (true)
        {
            std::swap(*first, *it);
            ++first;

            if (++it == last)
            {
                break;
            }

            if (first == middle)
            {
                middle = it;
            }
        }

        Iterator res = first;

        if (first != middle)
        {
            it = middle;
            while (true)
            {
                std::swap(*first, *it);
                ++first;

                if (++it == last)
                {
                    if (first == middle)
                    {
                        break;
                    }

                    it = middle;
                }
                else if (first == middle)
                {
                    middle = it;
                }
            }
        }

        return res;
    }
}