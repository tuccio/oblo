#pragma once

#include <oblo/core/handle.hpp>
#include <oblo/core/types.hpp>

namespace oblo::ui
{
    struct rect;
    struct transition_config;
    struct animated_values;

    class transition_store;

    struct layout_element;
    struct layout_state;

    using layout_id = h32<layout_element>;
}