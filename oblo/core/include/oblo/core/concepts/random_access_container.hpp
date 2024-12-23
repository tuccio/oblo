#pragma once

#include <type_traits>

namespace oblo
{
    template <typename T, typename V>
    concept random_access_container_of = requires(const T& v, const V* p, T::size_type s) {
        {
            p = &v[s]
        };
        {
            s = v.size()
        };
    };

    template <typename T>
    concept random_access_container =
        std::is_bounded_array_v<T> || random_access_container_of<T, typename T::value_type>;
}