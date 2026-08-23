#pragma once

#include <oblo/core/buffered_array.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/expected.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/ui/texture.hpp>

#include <freetype/freetype.h>

namespace oblo::ui
{
    struct font;
    using font_id = h16<font>;

    struct font_texture
    {
        u32 pitch;
        u32 rows;
        dynamic_array<u8> data;
    };

    struct font_glyph
    {
        u16 width;
        u16 height;

        // Bounding box offsets relative to the text origin (pen position)
        u16 bearingX;
        u16 bearingY;

        u16 advanceX;

        bool cached;

        h32<font_texture> textureId;
        vec2 uv;
    };

    struct font_variant
    {
        font_id font;
        u16 size;
        dynamic_array<font_glyph> glyphs;
    };

    struct font
    {
        FT_Face face{};
        buffered_array<font_variant, 4> variants;
    };

    struct font_cache
    {
        FT_Library freetype{};
        dynamic_array<font_texture> textures;
        handle_flat_pool_dense_map<font, font, u16, 0> fonts;

        expected<> init();
        void shutdown();

        expected<font_id> load_font(const char* path, u16 defaultSize);

        expected<font_variant&> create_or_add_variant(font_id id, u16 size);

        expected<const font_glyph&> get_or_add_glyph(font_variant& variant, u32 codepoint)
        {
            const font_glyph& glyph = variant.glyphs[codepoint];

            if (glyph.cached) [[likely]]
            {
                return glyph;
            }

            return load_glyph_to_cache(variant, codepoint);
        }

        expected<const font_glyph&> load_glyph_to_cache(font_variant& variant, u32 codepoint);
    };
}