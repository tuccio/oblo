#pragma once

#include <initializer_list>
#include <span>

    namespace oblo
{
    template <typename T>
    std::span<const T> make_span_initializer(std::initializer_list<T> list)
    {
        return {list.begin(), list.end()};
    }
}