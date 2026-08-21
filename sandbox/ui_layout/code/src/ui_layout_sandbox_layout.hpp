#pragma once

#include <oblo/core/forward.hpp>
#include <oblo/math/forward.hpp>

namespace oblo
{
    struct ui_layout_element_gpu;
}

namespace oblo::sandbox
{
    void ui_layout_set_size(vec2 size);

    void get_ui_layout_elements(dynamic_array<ui_layout_element_gpu>& out);

    void ui_layout_init();
    void ui_layout_update(time dt);
}