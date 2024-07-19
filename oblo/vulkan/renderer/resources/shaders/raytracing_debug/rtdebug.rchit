#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_ray_tracing : require

#include <renderer/debug>

layout(location = 0) rayPayloadInEXT vec3 r_HitColor;

void main()
{
    r_HitColor = debug_color_map(uint(gl_InstanceCustomIndexEXT));
}