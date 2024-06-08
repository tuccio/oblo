#pragma once

#include <oblo/core/iterator/iterator_range.hpp>

#include <iterator>

namespace oblo
{
    template <typename Container>
    auto reverse_range(Container&& c)
    {
        return iterator_range{std::make_reverse_iterator(std::end(c)), std::make_reverse_iterator(std::begin(c))};
    }
}