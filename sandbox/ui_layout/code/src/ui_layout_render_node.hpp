#pragma once

#include <oblo/gpu/forward.hpp>
#include <oblo/gpu/staging_buffer.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/math/vec4.hpp>
#include <oblo/renderer/graph/forward.hpp>
#include <oblo/renderer/graph/pins.hpp>
#include <oblo/ui/ui.hpp>

#include <ui_layout_atlas_cache.hpp>

#include <span>

namespace oblo
{
    // A single UI draw primitive in GPU-friendly form. Matches the std430 layout consumed
    // by the rasterize vertex shader. Solid rectangles use a textureId of 0; textured quads
    // (glyphs) carry the bindless handle of their atlas and a uvRect.
    struct ui_instance
    {
        vec4 rect;         // x, y, width, height (pixels, top-left origin)
        vec4 color;        // RGBA in [0, 1]
        vec4 cornerRadius; // TL, TR, BR, BL
        vec4 uvRect;       // normalized atlas rect (x, y, w, h); w == 0 for solid rects
        u32 textureId;     // bindless handle (h32<resident_texture>), or 0 for a solid rect
        u32 pad0;
        u32 pad1;
        u32 pad2;
    };

    static_assert(sizeof(ui_instance) == 80);

    // Rasterizes flat lists of ui::draw_command (rectangles + textured glyph quads) into a
    // texture. Rectangles and glyphs are packed into a single instance buffer and drawn as
    // triangle fans; atlas textures back the textured quads.
    struct ui_layout_render_node
    {
        pin::data<vec2u> inResolution;
        pin::data<span<const ui::draw_command>> inDrawCommands;
        pin::data<span<const ui::texture>> inTextures;
        pin::data<span<const ui::texture_command>> inTextureCommands;
        pin::data<ui_atlas_cache*> inAtlasCache;

        pin::texture outImage;

        pin::buffer instanceBuffer;

        h32<render_pass> rasterizePass;
        h32<render_pass_instance> rasterizePassInstance;

        void init(const frame_graph_init_context& ctx);
        void build(const frame_graph_build_context& ctx);
        void execute(const frame_graph_execute_context& ctx);
    };
}
