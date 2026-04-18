#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require

#include <pathtracing/pathtracing>
#include <renderer/textures>
#include <skybox/skybox_utility>

layout(location = 0) rayPayloadInEXT pathtracing_payload r_Payload;

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
    const vec4 color = texture_sample_2d_lod(g_SkyboxTexture, OBLO_SAMPLER_LINEAR_REPEAT, uv, 0);

    r_Payload.radiance = color.xyz * g_SkyboxMultiplier;
}