#pragma once

#include <oblo/math/vec2u.hpp>
#include <oblo/math/vec4.hpp>
#include <oblo/renderer/graph/forward.hpp>
#include <oblo/renderer/graph/pins.hpp>

#include <span>

namespace oblo
{
    // A single layout element in a GPU friendly format. Layout coordinates are in pixels,
    // with the origin at the top-left of the image, matching ui::rect.
    struct ui_layout_element_gpu
    {
        vec4 rect;         // x, y, width, height
        vec4 color;        // RGBA, channels in [0, 1]
        vec4 cornerRadius; // TL, TR, BR, BL
    };

    // Rasterizes a flat list of ui_layout_element_gpu into a texture. The rasterization
    // itself (drawing the rects, honoring colors and corner radii) is left as a TODO in
    // execute(), the compute shader is a placeholder.
    struct ui_layout_render_node
    {
        pin::data<vec2u> inResolution;
        pin::data<std::span<const ui_layout_element_gpu>> inElements;

        pin::texture outImage;

        pin::buffer elementsBuffer;

        h32<render_pass> rasterizePass;
        h32<render_pass_instance> rasterizePassInstance;

        void init(const frame_graph_init_context& ctx);
        void build(const frame_graph_build_context& ctx);
        void execute(const frame_graph_execute_context& ctx);
    };
}