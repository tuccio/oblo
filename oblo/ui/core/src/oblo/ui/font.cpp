#include <oblo/ui/font.hpp>

#include <oblo/core/filesystem/file.hpp>
#include <oblo/core/utility.hpp>

namespace oblo::ui
{
    constexpr u32 glyph_atlas_padding = 1;

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

    font_atlas& font_cache::create_atlas()
    {
        const u32 resolution = textureAtlasResolution ? textureAtlasResolution : 1024;

        const auto id = textures->create_texture(resolution, resolution, texture_format::r8_unorm);

        textures->commands.push_back({
            .kind = texture_command_kind::create,
            .create =
                {
                    .id = id,
                    .width = resolution,
                    .height = resolution,
                    .format = texture_format::r8_unorm,
                },
        });

        auto& atlas = m_atlases.push_back_default();
        atlas.id = id;
        atlas.width = resolution;
        atlas.height = resolution;
        atlas.init_skyline();

        // Mark the wole texture as dirty to ensure we initialize the texture
        atlas.add_dirty(0, 0, resolution, resolution);

        return atlas;
    }

    expected<rendered_glyph> font_cache::get_rendered_glyph(const font_glyph_reference& ref, FT_Face face)
    {
        const auto glyph = get_or_add_glyph(ref, face);

        if (!glyph)
        {
            return glyph.error();
        }

        const auto& atlas = m_atlases[glyph->textureIndex];

        rendered_glyph result{};
        result.width = glyph->width;
        result.height = glyph->height;
        result.bearingX = i16(glyph->bearingX);
        result.bearingY = i16(glyph->bearingY);
        result.advanceX = glyph->advanceX;
        result.atlas = atlas.id;
        result.atlasWidth = atlas.width;
        result.atlasHeight = atlas.height;
        result.posX = glyph->texturePosX;
        result.posY = glyph->texturePosY;

        return result;
    }

    void font_cache::add_rendered_glyph(FT_Face face, font_glyph& glyph)
    {
        const u32 glyphWidth = glyph.width;
        const u32 glyphHeight = glyph.height;

        // Nothing to rasterize (e.g. whitespace or an empty glyph).
        if (glyphWidth == 0 || glyphHeight == 0)
        {
            return;
        }

        const u32 paddedWidth = glyphWidth + glyph_atlas_padding * 2;
        const u32 paddedHeight = glyphHeight + glyph_atlas_padding * 2;

        u32 x = 0;
        u32 y = 0;
        bool placed = false;

        for (auto& atlas : m_atlases)
        {
            if (atlas.try_place(paddedWidth, paddedHeight, x, y))
            {
                glyph.textureIndex = u16(&atlas - m_atlases.begin());
                placed = true;
                break;
            }
        }

        if (!placed)
        {
            auto& atlas = create_atlas();
            OBLO_ASSERT(atlas.try_place(paddedWidth, paddedHeight, x, y));

            glyph.textureIndex = narrow_cast<u16>(m_atlases.size() - 1);
        }

        auto& atlas = m_atlases[glyph.textureIndex];

        const u32 atlasX = x + glyph_atlas_padding;
        const u32 atlasY = y + glyph_atlas_padding;

        glyph.texturePosX = narrow_cast<u16>(atlasX);
        glyph.texturePosY = narrow_cast<u16>(atlasY);

        // Bump the skyline: the region [x, x + paddedWidth) now reaches y + paddedHeight.
        atlas.add_region(x, y + paddedHeight, paddedWidth);

        // Blit the FreeType bitmap (8-bit coverage) into the atlas texture data.
        texture* const atlasTexture = textures->find_texture(atlas.id);
        const FT_Bitmap& bitmap = face->glyph->bitmap;

        for (u32 row = 0; row < glyphHeight; ++row)
        {
            for (u32 col = 0; col < glyphWidth; ++col)
            {
                const u8 coverage = bitmap.buffer[row * u32(bitmap.pitch) + col];
                atlasTexture->data[(atlasY + row) * atlasTexture->rowPitch + (atlasX + col)] = coverage;
            }
        }

        atlas.add_dirty(atlasX, atlasY, glyphWidth, glyphHeight);
    }

    void font_cache::flush_atlas_uploads()
    {
        for (auto& atlas : m_atlases)
        {
            u32 x, y, w, h;

            if (atlas.get_dirty_rect(x, y, w, h))
            {
                textures->notify_upload_required(atlas.id, x, y, w, h);
                atlas.clear_dirty();
            }
        }
    }
}