#pragma once

#include <oblo/core/types.hpp>

#include <type_traits>

namespace oblo
{
    template <typename T, typename Allocator>
    T* allocate_n(Allocator& allocator, usize count)
        requires std::is_pod_v<T>
    {
        const auto totalSize = sizeof(T) * count;
        auto* const ptr = allocator.allocate(totalSize, alignof(T));

        // Poor man start_lifetime_as
        new (ptr) std::byte[totalSize];

        return static_cast<T*>(ptr);
    }
}