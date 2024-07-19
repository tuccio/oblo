#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_ray_tracing : require

#extension GL_EXT_debug_printf : enable

#include <raytracing_debug/common>

layout(location = 0) rayPayloadInEXT vec3 r_HitColor;

void main()
{
    r_HitColor = vec3(0);
}