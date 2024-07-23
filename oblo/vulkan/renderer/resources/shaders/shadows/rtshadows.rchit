#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT float r_HitVisibility;

#define OBLO_DEBUG_PRINTF 1

#if OBLO_DEBUG_PRINTF
#extension GL_EXT_debug_printf : enable
#endif

#include <renderer/debug/printf>

void main()
{
    // printf_block_begin(debug_is_center());

    // // printf_text("L: ");
    // // printf_vec3(L);

    // // printf_text("positionWS: ");
    // // printf_vec3(positionWS);

    // printf_text("tMax: ");
    // printf_float(gl_HitTEXT);

    // printf_block_end();

    r_HitVisibility = 0.f;
}