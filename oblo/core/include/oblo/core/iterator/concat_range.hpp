#pragma once

#include <oblo/core/iterator/concat_iterator.hpp>
#include <oblo/core/iterator/iterator_range.hpp>

#include <tuple>
#include <utility>

namespace oblo
{
    template <typename... Containers>
    OBLO_FORCEINLINE auto concat_range(Containers&&... c)
    {
        return iterator_range{
            concat_iterator{
                std::make_tuple(std::begin(c)...),
                std::make_tuple(std::end(c)...),
                0,
            },
            concat_iterator{
                std::make_tuple(std::begin(c)...),
                std::make_tuple(std::end(c)...),
                sizeof...(Containers),
            },
        };
    }
}