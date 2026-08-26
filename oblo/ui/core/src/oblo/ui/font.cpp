#include <oblo/ui/font.hpp>

#include <oblo/core/filesystem/file.hpp>

namespace oblo::ui
{
    expected<> font_cache::init()
    {
        if (FT_Init_FreeType(&freetype))
        {
            return "Failed to initialze FreeType"_err;
        }

        return no_error;
    }

    void font_cache::shutdown()
    {
        if (freetype)
        {
            // Not entirely sure if this is necessary
            for (auto& font : fonts)
            {
                FT_Done_Face(font.face);
            }

            fonts.clear();
            glyphs.clear();

            FT_Done_FreeType(freetype);
            freetype = {};
        }
    }

    expected<font_id> font_cache::load_font_from_file(cstring_view path)
    {
        dynamic_array<byte> data;
        const expected result = filesystem::load_binary_file_into_memory(data, path);

        if (!result)
        {
            return result.error();
        }

        return load_font_from_memory(*result);
    }

    expected<font_id> font_cache::load_font_from_memory(span<const byte> data)
    {
        FT_Face face{};

        if (FT_New_Memory_Face(freetype, reinterpret_cast<const FT_Byte*>(data.data()), data.size(), 0, &face))
        {
            return "FreeType failed to load font file"_err;
        }

        fonts.emplace_back(face);
        return font_id{narrow_cast<u16>(fonts.size())};
    }

    void font_cache::add_rendered_glyph(FT_Face face, font_glyph& glyph)
    {
        (void) face;
        (void) glyph;

        // TODO: If a texture exists, find a spot to add the glyph. If none available create new one.
    }
}