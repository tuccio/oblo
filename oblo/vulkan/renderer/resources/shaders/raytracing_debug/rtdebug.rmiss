#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require

#include <renderer/textures>
#include <skybox/skybox>

layout(location = 0) rayPayloadInEXT vec3 r_HitColor;

layout(binding = 3) uniform b_SkyboxSettings
{
    vec3 g_SkyboxMultiplier;
    uint g_SkyboxTexture;
};

void main()
{
    // Generate sphere UVs to sample the skybox
    const vec2 uv = skybox_uv_from_ray_direction(gl_WorldRayDirectionEXT);

    const uint lod = 0;
    const vec4 color = texture_sample_2d_lod(g_SkyboxTexture, OBLO_SAMPLER_LINEAR, uv, 0);

    r_HitColor = color.xyz * g_SkyboxMultiplier;
}