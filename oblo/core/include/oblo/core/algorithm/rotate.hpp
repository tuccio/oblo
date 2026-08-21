#pragma once

#include <iterator>
#include <utility>

namespace oblo
{
    /// \brief Performs a left rotation on a range of elements.
    /// Swaps the elements in the range [first, last) in such a way that the elements in [first, middle) are placed
    /// after the elements in [middle, last) while the orders of the elements in both ranges are preserved. \remarks
    /// Equivalent to std::rotate.
    // Simple implementation of std::rotate for forward iterators, random access could be smarter.
    template <std::forward_iterator Iterator>
    Iterator rotate(Iterator first, Iterator middle, Iterator last)
    {
        if (first == middle)
        {
            return last;
        }

        if (middle == last)
        {
            return first;
        }

        Iterator it = middle;

        while (true)
        {
            using namespace std;
            swap(*first, *it);

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
                using namespace std;
                swap(*first, *it);

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