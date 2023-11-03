#pragma once

#include <oblo/core/handle.hpp>
#include <oblo/core/types.hpp>

#include <utility>

template <typename Tag, typename Value>
struct std::hash<oblo::handle<Tag, Value>>
{
    constexpr auto operator()(oblo::handle<Tag, Value> h) const noexcept
    {
        return std::hash<Value>{}(h.value);
    }
};