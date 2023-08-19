#pragma once

#include <type_traits>

namespace oblo::ecs
{
    template <typename T>
    struct is_tag : std::is_empty<T>
    {
    };

    template <typename T>
    static constexpr bool is_tag_v = is_tag<T>::value;
};
