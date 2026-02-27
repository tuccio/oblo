#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT float r_HitVisibility;

void main()
{
    r_HitVisibility = 1.f;
}