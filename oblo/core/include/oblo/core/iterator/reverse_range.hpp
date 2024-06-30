#pragma once

#include <oblo/core/iterator/iterator_range.hpp>
#include <oblo/core/iterator/reverse_iterator.hpp>

namespace oblo
{
    template <typename Container>
    auto reverse_range(Container&& c)
    {
        return iterator_range{rbegin(c), rend(c)};
    }
}