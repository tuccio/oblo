#pragma once

#include <initializer_list>
#include <span>

namespace oblo
{
    template <typename T, usize Extent = std::dynamic_extent>
    using span = std::span<T, Extent>;

    template <typename T>
    span<const T> make_span_initializer(std::initializer_list<T> list)
    {
        return {list.begin(), list.end()};
    }
}