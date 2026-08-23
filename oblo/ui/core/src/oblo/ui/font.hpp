#pragma once

#include <oblo/core/buffered_array.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/expected.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/core/reflection/fields.hpp>
#include <oblo/core/span.hpp>
#include <oblo/core/unordered_map.hpp>

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
    };

    struct font_glyph_reference
    {
        font_id font;
        u16 size;

        // This should be a result of FT_Get_Char_Index (or come from harfbuzz or something like that)
        FT_UInt glyphIndex;

        bool operator==(const font_glyph_reference&) const noexcept = default;
    };

    OBLO_FORCEINLINE hash_type hash_value(const font_glyph_reference& g)
    {
        static_assert(!struct_has_padding<font_glyph_reference>());
        return hash<u64>{}(std::bit_cast<u64>(g));
    }

    struct font
    {
        FT_Face face{};
        u32 currentPixelSize{0};
    };

    struct font_cache
    {
        FT_Library freetype{};

        dynamic_array<font> fonts;

        unordered_map<font_glyph_reference, font_glyph> glyphs;

        expected<> init();
        void shutdown();

        expected<font_id> load_font_from_file(cstring_view path);
        expected<font_id> load_font_from_memory(span<const byte> data);

        FT_Face find_font(font_id id) const
        {
            if (!id)
            {
                return nullptr;
            }

            return fonts[id.value - 1].face;
        }

        expected<const font_glyph&> get_or_add_glyph(const font_glyph_reference& ref, FT_Face face)
        {
            const auto [it, inserted] = glyphs.emplace(ref, font_glyph{});

            if (inserted)
            {
                font& f = fonts[ref.font.value - 1];

                if (f.currentPixelSize != ref.size)
                {
                    FT_Set_Pixel_Sizes(face, 0, ref.size);
                    f.currentPixelSize = ref.size;
                }

                const FT_Error error = FT_Load_Glyph(face, ref.glyphIndex, FT_LOAD_NO_BITMAP);

                if (error)
                {
                    return "FreeType failed to load glyph"_err;
                }

                const FT_Glyph_Metrics& metrics = face->glyph->metrics;

                it->second.width = narrow_cast<u16>(metrics.width >> 6);
                it->second.height = narrow_cast<u16>(metrics.height >> 6);
                it->second.bearingX = narrow_cast<u16>(metrics.horiBearingX >> 6);
                it->second.bearingY = narrow_cast<u16>(metrics.horiBearingY >> 6);
                it->second.advanceX = narrow_cast<u16>(metrics.horiAdvance >> 6);
            }

            return it->second;
        }
    };
}