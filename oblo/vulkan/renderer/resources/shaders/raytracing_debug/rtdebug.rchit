#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_ray_tracing : require

#include <raytracing_debug/common>

layout(location = 0) rayPayloadEXT hit_payload r_HitPayload;

void main()
{
    r_HitPayload.customIndex = gl_InstanceCustomIndexEXT;
}