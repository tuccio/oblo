#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include <renderer/lights>
#include <renderer/textures>
#include <renderer/volumes>

layout(location = 0) flat in uint in_InstanceId;
layout(location = 1) in vec3 in_PositionWS;
layout(location = 2) in vec3 in_Normal;
layout(location = 3) in vec2 in_UV0;

layout(location = 0) out vec4 out_Color;
layout(location = 1) out uint out_PickingId;

struct gpu_material
{
    vec3 albedo;
    uint albedoTexture;
};

#ifdef OBLO_PICKING_ENABLED

layout(std430, binding = 12) restrict readonly buffer i_EntityIdBuffer
{
    uint entityIds[];
};

#endif

layout(std430, binding = 2) restrict readonly buffer i_MaterialBuffer
{
    gpu_material materials[];
};

layout(binding = 3) uniform b_LightConfig
{
    light_config g_LightConfig;
};

layout(std430, binding = 4) restrict readonly buffer b_LightData
{
    light_data lights[];
};

vec3 light_contribution(in light_data light, in vec3 positionWS, in vec3 N)
{
    float attenuation = 1.f;
    vec3 L;

    if (light.type == OBLO_LIGHT_TYPE_DIRECTIONAL)
    {
        L = -light.direction;
    }
    else
    {
        const vec3 unnormalizedLightVector = light.position - positionWS;
        L = normalize(unnormalizedLightVector);
        attenuation = light_distance_attenuation(unnormalizedLightVector, light.invSqrRadius);
        attenuation *= light_angle_attenuation(light.direction, L, light.lightAngleScale, light.lightAngleOffset);
    }

    const float NdotL = saturate(dot(N, L));
    return NdotL * attenuation * light.intensity;
}

void main()
{
    const gpu_material material = materials[in_InstanceId];

    const vec4 color = sample_texture_2d(material.albedoTexture, OBLO_SAMPLER_LINEAR, in_UV0);

    vec3 reflected = vec3(0, 0, 0);

    for (uint lightIndex = 0; lightIndex < g_LightConfig.lightsCount; ++lightIndex)
    {
        reflected += light_contribution(lights[lightIndex], in_PositionWS, in_Normal);
    }

    out_Color = vec4(color.xyz * material.albedo * reflected, 1);
    // out_Color = vec4(in_Normal, 1);
    // out_Color = vec4(color.xyz * material.albedo, 1);

#ifdef OBLO_PICKING_ENABLED
    out_PickingId = entityIds[in_InstanceId];
#endif
}