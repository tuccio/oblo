#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_ray_tracing : require

layout(location = 1) rayPayloadInEXT bool r_IsShadowed;

void main()
{
    // Nothing was hit, no shadow to cast
    r_IsShadowed = false;
}