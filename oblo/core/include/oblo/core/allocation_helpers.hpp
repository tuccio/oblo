#pragma once

#include <oblo/core/lifetime.hpp>
#include <oblo/core/types.hpp>

#include <span>
#include <type_traits>

namespace oblo
{
    template <typename T, typename Allocator>
    [[nodiscard]] T* allocate_n(Allocator& allocator, usize count)
        requires std::is_pod_v<T>
    {
        if (count == 0)
        {
            return nullptr;
        }

        const auto totalSize = sizeof(T) * count;
        auto* const ptr = allocator.allocate(totalSize, alignof(T));

        return start_lifetime_as_array<T>(ptr, count);
    }

    template <typename T, typename Allocator>
    [[nodiscard]] std::span<T> allocate_n_span(Allocator& allocator, usize count)
        requires std::is_pod_v<T>
    {
        return {allocate_n<T>(allocator, count), count};
    }
}