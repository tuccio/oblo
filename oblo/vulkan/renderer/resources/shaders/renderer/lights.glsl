#ifndef OBLO_INCLUDE_RENDERER_LIGHTS
#define OBLO_INCLUDE_RENDERER_LIGHTS

#include <renderer/math>

// This has to match the light_type enums in the engine
#define OBLO_LIGHT_TYPE_POINT 0
#define OBLO_LIGHT_TYPE_SPOT 1
#define OBLO_LIGHT_TYPE_DIRECTIONAL 2

struct light_data
{
    vec3 position;
    float invSqrRadius;
    vec3 direction;
    uint type;
    vec3 intensity;
    float lightAngleScale;
    float lightAngleOffset;
};

struct light_config
{
    uint lightsCount;
};

// Based on Moving Frostbite to PBR
// https://media.contentapi.ea.com/content/dam/eacom/frostbite/files/course-notes-moving-frostbite-to-pbr-v2.pdf
float light_smooth_distance_attenuation(in float sqrDistance, in float invSqrRadius)
{
    const float factor = sqrDistance * invSqrRadius;
    const float smoothFactor = saturate(1.f - factor * factor);
    return smoothFactor * smoothFactor;
}

float light_distance_attenuation(in vec3 unnormalizedLightVector, in float invSqrRadius)
{
    const float bias = 0.01f;
    const float bias2 = bias * bias;

    const float sqrDistance = dot(unnormalizedLightVector, unnormalizedLightVector);
    const float attenuation = 1.0f / max(sqrDistance, bias2);

    return attenuation * light_smooth_distance_attenuation(sqrDistance, invSqrRadius);
}

float light_angle_attenuation(
    in vec3 lightDirection, in vec3 normalizedLightVector, in float lightAngleScale, in float lightAngleOffset)
{
    float cd = dot(lightDirection, normalizedLightVector);
    const float attenuation = saturate(cd * lightAngleScale + lightAngleOffset);
    return attenuation * attenuation;
}

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

#endif