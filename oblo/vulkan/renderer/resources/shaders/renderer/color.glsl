#ifndef OBLO_INCLUDE_RENDERER_COLOR
#define OBLO_INCLUDE_RENDERER_COLOR

#include <renderer/math>

const float g_DisplayGamma = 2.2f;
const float g_InvDisplayGamma = 1.f / g_DisplayGamma;

vec4 srgb_to_linear(uint srgb)
{
    const vec4 color = unpackUnorm4x8(srgb);
    return vec4(pow(color.xyz, g_DisplayGamma), color.w);
}

vec3 linear_to_srgb_float(vec3 linear)
{
    return pow(linear, g_InvDisplayGamma);
}

#endif