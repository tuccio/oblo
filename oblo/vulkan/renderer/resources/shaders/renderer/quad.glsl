#ifndef OBLO_INCLUDE_RENDERER_QUAD
#define OBLO_INCLUDE_RENDERER_QUAD

// Source:
// https://github.com/GPUOpen-Effects/FidelityFX-Denoiser/blob/master/ffx-shadows-dnsr/ffx_denoiser_shadows_util.h
//  LANE TO 8x8 MAPPING
//  ===================
//  00 01 08 09 10 11 18 19
//  02 03 0a 0b 12 13 1a 1b
//  04 05 0c 0d 14 15 1c 1d
//  06 07 0e 0f 16 17 1e 1f
//  20 21 28 29 30 31 38 39
//  22 23 2a 2b 32 33 3a 3b
//  24 25 2c 2d 34 35 3c 3d
//  26 27 2e 2f 36 37 3e 3f
uint quad_bitfield_extract(uint src, uint off, uint bits)
{
    uint mask = (1u << bits) - 1;
    return (src >> off) & mask;
} // ABfe

uint quad_bitfield_insert(uint src, uint ins, uint bits)
{
    uint mask = (1u << bits) - 1;
    return (ins & mask) | (src & (~mask));
} // ABfiM

/// Remaps a linear int in [0, 63] to a 8x8 grid in morton order
uvec2 quad_remap_lane_8x8(uint lane)
{
    return uvec2(quad_bitfield_insert(quad_bitfield_extract(lane, 2u, 3u), lane, 1u),
        quad_bitfield_insert(quad_bitfield_extract(lane, 3u, 3u), quad_bitfield_extract(lane, 1u, 2u), 2u));
}

#endif