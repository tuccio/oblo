#pragma once

#include <oblo/core/string/string_view.hpp>
#include <oblo/renderer/graph/frame_graph_registry.hpp>
#include <oblo/renderer/graph/frame_graph_template.hpp>

namespace oblo::ui_layout_view
{
    constexpr string_view InResolution{"Resolution"};
    constexpr string_view InElements{"Elements"};

    constexpr string_view OutLayoutImage{"LayoutImage"};

    // A graph containing a single ui_layout_render_node. Instantiate it per viewport and
    // feed it the resolution and the list of layout elements every frame. The output
    // texture can be viewed from the editor's "Frame Graph Debug" window, or presented to
    // the swapchain via graphics_window_context::set_output.
    frame_graph_template create(const frame_graph_registry& registry);
}