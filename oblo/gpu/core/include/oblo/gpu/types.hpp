#pragma once

#include <oblo/core/types.hpp>

namespace oblo::gpu
{
    enum class mesh_index_type : u8
    {
        none,
        u8,
        u16,
        u32,
    };

    enum class texture_usage : u8
    {
        render_target_write,
        depth_stencil_read,
        depth_stencil_write,
        shader_read,
        storage_read,
        storage_write,
        transfer_source,
        transfer_destination,
        present,
        enum_max,
    };

    enum class buffer_usage : u8
    {
        storage_read,
        storage_write,
        /// @brief This means the buffer is not actually used on GPU in this node, just uploaded on.
        storage_upload,
        uniform,
        indirect,
        download,
        index,
        enum_max,
    };

    enum class attachment_load_op : u8
    {
        none,
        load,
        clear,
        dont_care,
    };

    enum class attachment_store_op : u8
    {
        none,
        store,
        dont_care,
    };

    enum class blend_factor : u8
    {
        zero,
        one,
        src_color,
        one_minus_src_color,
        dst_color,
        one_minus_dst_color,
        src_alpha,
        one_minus_src_alpha,
        dst_alpha,
        one_minus_dst_alpha,
        constant_color,
        one_minus_constant_color,
        constant_alpha,
        one_minus_constant_alpha,
        src_alpha_saturate,
        src1_color,
        one_minus_src1_color,
        src1_alpha,
        one_minus_src1_alpha,
    };

    // @brief Blend operation, enumerator values match VkBlendOp.
    enum class blend_op : u32
    {
        add,
        subtract,
        reverse_subtract,
        min,
        max,
        zero_ext = 1000148000,
        src_ext = 1000148001,
        dst_ext = 1000148002,
        src_over_ext = 1000148003,
        dst_over_ext = 1000148004,
        src_in_ext = 1000148005,
        dst_in_ext = 1000148006,
        src_out_ext = 1000148007,
        dst_out_ext = 1000148008,
        src_atop_ext = 1000148009,
        dst_atop_ext = 1000148010,
        xor_ext = 1000148011,
        multiply_ext = 1000148012,
        screen_ext = 1000148013,
        overlay_ext = 1000148014,
        darken_ext = 1000148015,
        lighten_ext = 1000148016,
        colordodge_ext = 1000148017,
        colorburn_ext = 1000148018,
        hardlight_ext = 1000148019,
        softlight_ext = 1000148020,
        difference_ext = 1000148021,
        exclusion_ext = 1000148022,
        invert_ext = 1000148023,
        invert_rgb_ext = 1000148024,
        lineardodge_ext = 1000148025,
        linearburn_ext = 1000148026,
        vividlight_ext = 1000148027,
        linearlight_ext = 1000148028,
        pinlight_ext = 1000148029,
        hardmix_ext = 1000148030,
        hsl_hue_ext = 1000148031,
        hsl_saturation_ext = 1000148032,
        hsl_color_ext = 1000148033,
        hsl_luminosity_ext = 1000148034,
        plus_ext = 1000148035,
        plus_clamped_ext = 1000148036,
        plus_clamped_alpha_ext = 1000148037,
        plus_darker_ext = 1000148038,
        minus_ext = 1000148039,
        minus_clamped_ext = 1000148040,
        contrast_ext = 1000148041,
        invert_ovg_ext = 1000148042,
        red_ext = 1000148043,
        green_ext = 1000148044,
        blue_ext = 1000148045,
    };

    enum class color_component
    {
        r,
        g,
        b,
        a,
        enum_max,
    };

    /// @brief Compare operation, enumerator values match VkCompareOp.
    enum class compare_op
    {
        never,
        less,
        equal,
        less_or_equal,
        greater,
        not_equal,
        greater_or_equal,
        always,
    };

    /// @brief Stencil operation, enumerator values match VkStencilOp.
    enum class stencil_op
    {
        keep,
        zero,
        replace,
        increment_and_clamp,
        decrement_and_clamp,
        invert,
        increment_and_wrap,
        decrement_and_wrap,
    };

    /// @brief Texture format, enumerator values match VkFormat.
    enum class texture_format : u32
    {
        undefined = 0,
        r4g4_unorm_pack8 = 1,
        r4g4b4a4_unorm_pack16 = 2,
        b4g4r4a4_unorm_pack16 = 3,
        r5g6b5_unorm_pack16 = 4,
        b5g6r5_unorm_pack16 = 5,
        r5g5b5a1_unorm_pack16 = 6,
        b5g5r5a1_unorm_pack16 = 7,
        a1r5g5b5_unorm_pack16 = 8,
        r8_unorm = 9,
        r8_snorm = 10,
        r8_uscaled = 11,
        r8_sscaled = 12,
        r8_uint = 13,
        r8_sint = 14,
        r8_srgb = 15,
        r8g8_unorm = 16,
        r8g8_snorm = 17,
        r8g8_uscaled = 18,
        r8g8_sscaled = 19,
        r8g8_uint = 20,
        r8g8_sint = 21,
        r8g8_srgb = 22,
        r8g8b8_unorm = 23,
        r8g8b8_snorm = 24,
        r8g8b8_uscaled = 25,
        r8g8b8_sscaled = 26,
        r8g8b8_uint = 27,
        r8g8b8_sint = 28,
        r8g8b8_srgb = 29,
        b8g8r8_unorm = 30,
        b8g8r8_snorm = 31,
        b8g8r8_uscaled = 32,
        b8g8r8_sscaled = 33,
        b8g8r8_uint = 34,
        b8g8r8_sint = 35,
        b8g8r8_srgb = 36,
        r8g8b8a8_unorm = 37,
        r8g8b8a8_snorm = 38,
        r8g8b8a8_uscaled = 39,
        r8g8b8a8_sscaled = 40,
        r8g8b8a8_uint = 41,
        r8g8b8a8_sint = 42,
        r8g8b8a8_srgb = 43,
        b8g8r8a8_unorm = 44,
        b8g8r8a8_snorm = 45,
        b8g8r8a8_uscaled = 46,
        b8g8r8a8_sscaled = 47,
        b8g8r8a8_uint = 48,
        b8g8r8a8_sint = 49,
        b8g8r8a8_srgb = 50,
        a8b8g8r8_unorm_pack32 = 51,
        a8b8g8r8_snorm_pack32 = 52,
        a8b8g8r8_uscaled_pack32 = 53,
        a8b8g8r8_sscaled_pack32 = 54,
        a8b8g8r8_uint_pack32 = 55,
        a8b8g8r8_sint_pack32 = 56,
        a8b8g8r8_srgb_pack32 = 57,
        a2r10g10b10_unorm_pack32 = 58,
        a2r10g10b10_snorm_pack32 = 59,
        a2r10g10b10_uscaled_pack32 = 60,
        a2r10g10b10_sscaled_pack32 = 61,
        a2r10g10b10_uint_pack32 = 62,
        a2r10g10b10_sint_pack32 = 63,
        a2b10g10r10_unorm_pack32 = 64,
        a2b10g10r10_snorm_pack32 = 65,
        a2b10g10r10_uscaled_pack32 = 66,
        a2b10g10r10_sscaled_pack32 = 67,
        a2b10g10r10_uint_pack32 = 68,
        a2b10g10r10_sint_pack32 = 69,
        r16_unorm = 70,
        r16_snorm = 71,
        r16_uscaled = 72,
        r16_sscaled = 73,
        r16_uint = 74,
        r16_sint = 75,
        r16_sfloat = 76,
        r16g16_unorm = 77,
        r16g16_snorm = 78,
        r16g16_uscaled = 79,
        r16g16_sscaled = 80,
        r16g16_uint = 81,
        r16g16_sint = 82,
        r16g16_sfloat = 83,
        r16g16b16_unorm = 84,
        r16g16b16_snorm = 85,
        r16g16b16_uscaled = 86,
        r16g16b16_sscaled = 87,
        r16g16b16_uint = 88,
        r16g16b16_sint = 89,
        r16g16b16_sfloat = 90,
        r16g16b16a16_unorm = 91,
        r16g16b16a16_snorm = 92,
        r16g16b16a16_uscaled = 93,
        r16g16b16a16_sscaled = 94,
        r16g16b16a16_uint = 95,
        r16g16b16a16_sint = 96,
        r16g16b16a16_sfloat = 97,
        r32_uint = 98,
        r32_sint = 99,
        r32_sfloat = 100,
        r32g32_uint = 101,
        r32g32_sint = 102,
        r32g32_sfloat = 103,
        r32g32b32_uint = 104,
        r32g32b32_sint = 105,
        r32g32b32_sfloat = 106,
        r32g32b32a32_uint = 107,
        r32g32b32a32_sint = 108,
        r32g32b32a32_sfloat = 109,
        r64_uint = 110,
        r64_sint = 111,
        r64_sfloat = 112,
        r64g64_uint = 113,
        r64g64_sint = 114,
        r64g64_sfloat = 115,
        r64g64b64_uint = 116,
        r64g64b64_sint = 117,
        r64g64b64_sfloat = 118,
        r64g64b64a64_uint = 119,
        r64g64b64a64_sint = 120,
        r64g64b64a64_sfloat = 121,
        b10g11r11_ufloat_pack32 = 122,
        e5b9g9r9_ufloat_pack32 = 123,
        d16_unorm = 124,
        x8_d24_unorm_pack32 = 125,
        d32_sfloat = 126,
        s8_uint = 127,
        d16_unorm_s8_uint = 128,
        d24_unorm_s8_uint = 129,
        d32_sfloat_s8_uint = 130,
        bc1_rgb_unorm_block = 131,
        bc1_rgb_srgb_block = 132,
        bc1_rgba_unorm_block = 133,
        bc1_rgba_srgb_block = 134,
        bc2_unorm_block = 135,
        bc2_srgb_block = 136,
        bc3_unorm_block = 137,
        bc3_srgb_block = 138,
        bc4_unorm_block = 139,
        bc4_snorm_block = 140,
        bc5_unorm_block = 141,
        bc5_snorm_block = 142,
        bc6h_ufloat_block = 143,
        bc6h_sfloat_block = 144,
        bc7_unorm_block = 145,
        bc7_srgb_block = 146,
        etc2_r8g8b8_unorm_block = 147,
        etc2_r8g8b8_srgb_block = 148,
        etc2_r8g8b8a1_unorm_block = 149,
        etc2_r8g8b8a1_srgb_block = 150,
        etc2_r8g8b8a8_unorm_block = 151,
        etc2_r8g8b8a8_srgb_block = 152,
        eac_r11_unorm_block = 153,
        eac_r11_snorm_block = 154,
        eac_r11g11_unorm_block = 155,
        eac_r11g11_snorm_block = 156,
        astc_4x4_unorm_block = 157,
        astc_4x4_srgb_block = 158,
        astc_5x4_unorm_block = 159,
        astc_5x4_srgb_block = 160,
        astc_5x5_unorm_block = 161,
        astc_5x5_srgb_block = 162,
        astc_6x5_unorm_block = 163,
        astc_6x5_srgb_block = 164,
        astc_6x6_unorm_block = 165,
        astc_6x6_srgb_block = 166,
        astc_8x5_unorm_block = 167,
        astc_8x5_srgb_block = 168,
        astc_8x6_unorm_block = 169,
        astc_8x6_srgb_block = 170,
        astc_8x8_unorm_block = 171,
        astc_8x8_srgb_block = 172,
        astc_10x5_unorm_block = 173,
        astc_10x5_srgb_block = 174,
        astc_10x6_unorm_block = 175,
        astc_10x6_srgb_block = 176,
        astc_10x8_unorm_block = 177,
        astc_10x8_srgb_block = 178,
        astc_10x10_unorm_block = 179,
        astc_10x10_srgb_block = 180,
        astc_12x10_unorm_block = 181,
        astc_12x10_srgb_block = 182,
        astc_12x12_unorm_block = 183,
        astc_12x12_srgb_block = 184,
    };

    /// @brief Primitive topology, enumerator values match VkPrimitiveTopology.
    enum class primitive_topology : u8
    {
        point_list,
        line_list,
        line_strip,
        triangle_list,
        triangle_strip,
        triangle_fan,
        line_list_with_adjacency,
        line_strip_with_adjacency,
        triangle_list_with_adjacency,
        triangle_strip_with_adjacency,
        patch_list,
    };

    /// @brief Polygon mode, enumerator values match VkPolygonMode.
    enum class polygon_mode : u8
    {
        fill,
        line,
        point,
    };

    /// @brief Front face, enumerator values match VkFrontFace.
    enum class front_face : u8
    {
        counter_clockwise,
        clockwise,
    };

    enum class cull_mode : u8
    {
        front,
        back,
        enum_max,
    };

    enum class pipeline_depth_stencil_state_create : u8
    {
        rasterization_order_attachment_depth_access,
        rasterization_order_attachment_stencil_access,
        enum_max,
    };

    enum class shader_stage : u8
    {
        mesh,
        task,
        compute,
        vertex,
        geometry,
        tessellation_control,
        tessellation_evaluation,
        fragment,
        raygen,
        intersection,
        closest_hit,
        any_hit,
        miss,
        callable,
        enum_max,
    };

    enum class shader_module_format : u8
    {
        spirv,
    };
}