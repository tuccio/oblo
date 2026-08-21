#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/math/vec2.hpp>

namespace oblo
{
    struct ui_layout_element_gpu;
}

namespace oblo::sandbox
{
    // Internal: sets the layout area used by the demo layout before ui_layout_update().
    // Not part of the script API.
    void ui_layout_set_size(vec2 size);

    void get_ui_layout_elements(dynamic_array<ui_layout_element_gpu>& out);

    // Script API functions (declared in ui_layout_sandbox_interface.hpp).
    void ui_layout_init();
    void ui_layout_update(f32 dt);
}