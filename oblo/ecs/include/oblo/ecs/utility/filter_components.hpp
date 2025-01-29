#pragma once

#include <tuple>
#include <type_traits>

namespace oblo::ecs
{
    template <typename... ComponentOrTags>
    struct filter_components
    {
        using tuple = decltype(std::tuple_cat(
            std::conditional_t<!std::is_empty_v<ComponentOrTags>, std::tuple<ComponentOrTags*>, std::tuple<>>{}...));
    };
}