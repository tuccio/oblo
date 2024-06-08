#pragma once

#include <oblo/core/iterator/iterator_range.hpp>
#include <oblo/core/iterator/zip_iterator.hpp>

namespace oblo
{
    template <typename... Containers>
    auto zip_range(Containers&&... c)
    {
        return iterator_range{zip_iterator{std::begin(c)...}, zip_iterator{std::end(c)...}};
    }
}