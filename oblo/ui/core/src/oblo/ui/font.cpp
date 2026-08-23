#include <oblo/ui/font.hpp>

#include <freetype/ftoutln.h>

namespace oblo::ui
{
    expected<> get_glyph_metrics(FT_Face face, u32 code)
    {
        FT_Error error;

        // 1. Load the glyph slot for the specific character
        // FT_LOAD_DEFAULT looks up the glyph index and loads its unrendered metrics
        error = FT_Load_Char(face, code, FT_LOAD_RENDER);

        if (error)
        {
            return "FreeType error"_err;
        }

        // 2. Access metrics from the glyph slot
        // CRITICAL: FreeType stores these metrics in 26.6 fractional pixels (1 unit = 1/64th pixel)
        FT_Glyph_Metrics metrics = face->glyph->metrics;

        // Convert from 26.6 fractional units to standard integer pixels (divide by 64 or bit-shift right by 6)
        const i32 width = metrics.width >> 6;
        const i32 height = metrics.height >> 6;
        const i32 hAdvance = metrics.horiAdvance >> 6;

        const i32 bearingX = metrics.horiBearingX >> 6;
        const i32 bearingX = metrics.horiBearingY >> 6;

        FT_Render_Glyph()

            return no_error;
    }

    FT_Error rasterize_directly_to_user_memory(
        FT_Library library, FT_Face face, unsigned char* my_sub_buffer, int width, int rows)
    {

        FT_Bitmap bmp{
            .rows = rows,
            .width = width,
            .pitch = width,                   // Assuming tightly packed 1 byte per pixel
            .buffer = my_sub_buffer,          // YOUR physical memory pointer
            .pixel_mode = FT_PIXEL_MODE_GRAY, // Grayscale mode
            .num_grays = 256,
        }

        // 3. Shift the outline geometry so it aligns correctly with your buffer's 0,0 origin
        // Because FT_Outline_Get_Bitmap doesn't automatically translate for bearing offsets
        FT_Outline_Translate(&face->glyph->outline,
            -face->glyph->metrics.horiBearingX,
            -face->glyph->metrics.horiBearingY);

        // 4. Force FreeType to render the math directly into your buffer pointer
        error = FT_Outline_Get_Bitmap(library, &face->glyph->outline, &bmp); //
        return error;
    }
}