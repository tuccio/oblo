#ifndef OBLO_INCLUDE_RENDERER_MOTION_VECTORS
#define OBLO_INCLUDE_RENDERER_MOTION_VECTORS

// We currently store motion vectors as 16-bit floats, so no encoding is necessary.
#define OBLO_MOTION_VECTORS_FORMAT rg16

vec2 motion_vectors_encode(vec2 ndc)
{
    return ndc;
}

vec2 motion_vectors_decode(vec2 texel)
{
    return texel;
}

#endif