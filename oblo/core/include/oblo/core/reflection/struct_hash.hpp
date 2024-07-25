#pragma once

#include <oblo/core/hash.hpp>
#include <oblo/core/struct_apply.hpp>

namespace oblo
{
    template <template <typename> typename Hash, typename T>
    constexpr auto struct_hash(const T& o)
    {
        if constexpr (requires(T v) { Hash<T>{}(v); })
        {
            return Hash<T>{}(o);
        }
        else
        {
            return struct_apply([](auto&&... m) { return hash_all<Hash>(sizeof...(m), struct_hash<Hash>(m)...); }, o);
        }
    }
}