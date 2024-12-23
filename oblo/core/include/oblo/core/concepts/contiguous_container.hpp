#pragma once

#include <type_traits>

namespace oblo
{
    template <typename T, typename V>
    concept contiguous_container_of = requires(const T& v, const V* p, T::size_type s) {
        {
            p = v.data()
        };
        {
            s = v.size()
        };
    };

    template <typename T>
    concept contiguous_container = contiguous_container_of<T, typename T::value_type>;
}