#pragma once

#include <oblo/core/iterator/iterator_range.hpp>
#include <oblo/core/iterator/zip_iterator.hpp>

namespace oblo
{
    template <typename T>
    auto deque_chunk_range(const deque<T>& d)
    {
        return iterator_range{
            deque_chunk_iterator<const T>{d.begin(), d.size()},
            deque_chunk_iterator<const T>{d.end(), d.size()},
        };
    }

    template <typename T>
    auto deque_chunk_range(deque<T>& d)
    {
        return iterator_range{
            deque_chunk_iterator<T>{d.begin(), d.size()},
            deque_chunk_iterator<T>{d.end(), d.size()},
        };
    }
}