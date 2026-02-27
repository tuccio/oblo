#ifndef OBLO_INCLUDE_RENDERER_MOTION_VECTORS
#define OBLO_INCLUDE_RENDERER_MOTION_VECTORS

// We currently store motion vectors as 16-bit floats, so no encoding is necessary.
#define OBLO_MOTION_VECTORS_FORMAT rg16

vec2 motion_vectors_encode(in vec2 ndc)
{
    return ndc;
}

vec2 motion_vectors_decode(in vec2 texel)
{
    return texel;
}

vec2 motion_vectors_reproject_uv(in vec4 texel, in uvec2 coords, in uvec2 resolution)
{
    // Assuming the coords are in the middle of the pixel, we obtain the uv of the current pixel
    const vec2 uv = (vec2(coords + .5f) / vec2(resolution));

    const vec2 motionVector = motion_vectors_decode(texel.xy);

    // Motion vectors are calculated as current - previous, so we need to subtract to obtain the previous coordinates
    // We need to divide by 2 because motion vecctors are in NDC space (i.e. [-1, 1]) and we want to move it to texture
    // space (which is [0, 1]).
    return uv - motionVector * .5f;
}

#endif