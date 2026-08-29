#pragma once

#include <oblo/core/string/string_view.hpp>
#include <oblo/renderer/graph/frame_graph_registry.hpp>
#include <oblo/renderer/graph/frame_graph_template.hpp>

namespace oblo::ui_layout_view
{
    constexpr string_view InResolution{"Resolution"};
    constexpr string_view InDrawCommands{"DrawCommands"};
    constexpr string_view InTextures{"Textures"};
    constexpr string_view InTextureCommands{"TextureCommands"};
    constexpr string_view InAtlasCache{"AtlasCache"};

    constexpr string_view OutLayoutImage{"LayoutImage"};

    // A graph containing a single ui_layout_render_node. Instantiate it per viewport and
    // feed it the resolution, the draw commands (rectangles + glyph quads) and the atlas
    // textures produced by oblo::ui::context every frame. The output texture can be viewed
    // from the editor's "Frame Graph Debug" window, or presented to the swapchain via
    // graphics_window_context::set_output.
    frame_graph_template create(const frame_graph_registry& registry);
}
