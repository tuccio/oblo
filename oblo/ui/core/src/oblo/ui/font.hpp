#pragma once

#include <oblo/core/buffered_array.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/expected.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/core/handle_flat_pool_set.hpp>
#include <oblo/core/reflection/fields.hpp>
#include <oblo/core/span.hpp>
#include <oblo/core/unordered_map.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/ui/texture_storage.hpp>

#include <freetype/freetype.h>

namespace oblo::ui
{
    struct texture_storage;

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

        // Bounding box offsets relative to the text origin (pen position). These can be
        // negative (e.g. italics), so they must be signed.
        i16 bearingX;
        i16 bearingY;

        u16 advanceX;

        u16 textureIndex;
        u16 texturePosX;
        u16 texturePosY;
    };

    struct font_glyph_reference
    {
        font_id font;
        u16 size;

        // This should be a result of FT_Get_Char_Index (or come from harfbuzz or something like that)
        FT_UInt glyphIndex;

        bool operator==(const font_glyph_reference&) const noexcept = default;
    };

    // Everything needed by the UI to turn a cached glyph into a textured quad.
    struct rendered_glyph
    {
        u16 width;
        u16 height;

        i16 bearingX;
        i16 bearingY;
        u16 advanceX;

        h32<texture> atlas;
        u32 atlasWidth;
        u32 atlasHeight;
        u16 posX;
        u16 posY;
    };

    // A single atlas texture gathering many rendered glyphs. Glyphs are placed with a
    // skyline (bottom-left) packer so we can grow the atlas horizontally and keep the
    // wasted vertical space low.
    struct font_atlas
    {
        h32<texture> id;
        u32 width;
        u32 height;

        struct skyline_node
        {
            u32 x;
            u32 y;
        };

        // Sorted list of (x, topHeight) nodes describing the top profile of the atlas.
        // Invariant: the first node is at x == 0 and the last at x == width.
        dynamic_array<skyline_node> skyline;

        u32 dirtyMinX{~0u};
        u32 dirtyMinY{~0u};
        u32 dirtyMaxX{0};
        u32 dirtyMaxY{0};
        bool isDirty{false};

        void init_skyline()
        {
            skyline.clear();
            skyline.push_back({0, 0});
            skyline.push_back({width, 0});

            clear_dirty();
        }

        // Expands the dirty rectangle to also cover [x, x + w) x [y, y + h).
        void add_dirty(u32 x, u32 y, u32 w, u32 h)
        {
            dirtyMinX = min(dirtyMinX, x);
            dirtyMinY = min(dirtyMinY, y);
            dirtyMaxX = max(dirtyMaxX, x + w);
            dirtyMaxY = max(dirtyMaxY, y + h);
            isDirty = true;
        }

        bool get_dirty_rect(u32& outX, u32& outY, u32& outW, u32& outH) const
        {
            if (!isDirty)
            {
                return false;
            }

            outX = dirtyMinX;
            outY = dirtyMinY;
            outW = dirtyMaxX - dirtyMinX;
            outH = dirtyMaxY - dirtyMinY;
            return true;
        }

        void clear_dirty()
        {
            dirtyMinX = ~0u;
            dirtyMinY = ~0u;
            dirtyMaxX = 0;
            dirtyMaxY = 0;
            isDirty = false;
        }

        bool try_place(u32 w, u32 h, u32& outX, u32& outY) const
        {
            u32 bestX = ~0u;
            u32 bestY = ~0u;

            for (u32 i = 0; i < skyline.size(); ++i)
            {
                const u32 x = skyline[i].x;

                if (x + w > width)
                {
                    continue;
                }

                u32 y = 0;

                for (u32 j = i; j < skyline.size() && skyline[j].x < x + w; ++j)
                {
                    y = max(y, skyline[j].y);
                }

                if (y + h <= height)
                {
                    if (bestX == ~0u || y < bestY || (y == bestY && x < bestX))
                    {
                        bestX = x;
                        bestY = y;
                    }
                }
            }

            if (bestX == ~0u)
            {
                return false;
            }

            outX = bestX;
            outY = bestY;
            return true;
        }

        // Records that a rect of the given width now occupies [x, x + w) up to height `top`.
        void add_region(u32 x, u32 top, u32 w)
        {
            const u32 right = x + w;

            // Drop any interior nodes; the placed rect redefines the profile there.
            for (u32 i = 0; i < skyline.size();)
            {
                if (skyline[i].x > x && skyline[i].x < right)
                {
                    skyline.erase(skyline.begin() + i);
                }
                else
                {
                    ++i;
                }
            }

            bool inserted = false;

            for (auto& n : skyline)
            {
                if (n.x == x)
                {
                    n.y = top;
                    inserted = true;
                    break;
                }
            }

            if (!inserted)
            {
                u32 idx = 0;
                while (idx < skyline.size() && skyline[idx].x < x)
                {
                    ++idx;
                }

                skyline.insert(skyline.begin() + idx, skyline_node{x, top});
            }

            if (right < width)
            {
                bool found = false;

                for (auto& n : skyline)
                {
                    if (n.x == right)
                    {
                        n.y = max(n.y, top);
                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    u32 idx = 0;
                    while (idx < skyline.size() && skyline[idx].x < right)
                    {
                        ++idx;
                    }

                    skyline.insert(skyline.begin() + idx, skyline_node{right, top});
                }
            }

            // Merge consecutive nodes that share the same height.
            for (u32 i = 0; i + 1 < skyline.size();)
            {
                if (skyline[i].y == skyline[i + 1].y)
                {
                    skyline.erase(skyline.begin() + i + 1);
                }
                else
                {
                    ++i;
                }
            }
        }
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

        u16 textureAtlasResolution{};

        texture_storage* textures{};

        dynamic_array<font_atlas> m_atlases;

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

        // Returns the render information of a glyph, rendering it into the atlas if needed.
        expected<rendered_glyph> get_rendered_glyph(const font_glyph_reference& ref, FT_Face face);

        font_atlas& create_atlas();

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

                const bool withFontTexture = textures && is_glyph_render_enabled();
                const auto glyphLoadFlags = FT_LOAD_DEFAULT | (withFontTexture ? FT_LOAD_RENDER : FT_LOAD_NO_BITMAP);

                const FT_Error error = FT_Load_Glyph(face, ref.glyphIndex, glyphLoadFlags);

                if (error)
                {
                    return "FreeType failed to load glyph"_err;
                }

                const FT_Glyph_Metrics& metrics = face->glyph->metrics;

                it->second.width = narrow_cast<u16>(metrics.width >> 6);
                it->second.height = narrow_cast<u16>(metrics.height >> 6);
                it->second.bearingX = narrow_cast<i16>(metrics.horiBearingX >> 6);
                it->second.bearingY = narrow_cast<i16>(metrics.horiBearingY >> 6);
                it->second.advanceX = narrow_cast<u16>(metrics.horiAdvance >> 6);

                if (withFontTexture)
                {
                    add_rendered_glyph(face, it->second);
                }
            }

            return it->second;
        }

        bool is_glyph_render_enabled() const noexcept
        {
            return textureAtlasResolution > 0;
        }

        void add_rendered_glyph(FT_Face face, font_glyph& glyph);

        void flush_atlas_uploads();
    };
}