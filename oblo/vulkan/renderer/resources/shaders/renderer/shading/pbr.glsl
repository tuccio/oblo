#ifndef OBLO_INCLUDE_RENDERER_SHADING_PBR
#define OBLO_INCLUDE_RENDERER_SHADING_PBR

#include <renderer/constants>
#include <renderer/material>
#include <renderer/math>

struct pbr_material
{
    vec3 albedo;
    float metalness;
    float roughness;
    float ior;
};

float pbr_distribution_ggx(in float NdotH, float alpha2)
{
    const float num = NdotH * alpha2;
    const float denom = NdotH * NdotH * (alpha2 - 1) + 1;
    return num / (float_pi() * denom * denom);
}

float pbr_fresnel_schlick(in float cosTheta, in float ior)
{
    const float F0num = ior - 1;
    const float F0denom = ior + 1;
    const float F0sqrt = (ior - 1) / (ior + 1);
    const float F0 = F0sqrt * F0sqrt;

    return F0 + (1 - F0) * pow5(1 - cosTheta);
}

float pbr_fresnel_diffuse_disney(in float cosTheta, in float F90)
{
    return 1 + (F90 - 1) * pow5(1 - cosTheta);
}

float pbr_f90(in float cosTheta, in float roughness)
{
    return .5f + 2 * roughness * pow2(cosTheta);
}

float pbr_shadowing_schlick_smith_ggx(in float NdotV, in float NdotL, float alpha2)
{
    const float Gv = NdotV / (NdotV * (1 - alpha2) + alpha2);
    const float Gl = NdotL / (NdotL * (1 - alpha2) + alpha2);

    return Gv * Gl;
}

vec3 pbr_brdf(in vec3 N, in vec3 V, in vec3 L, in pbr_material m)
{
    const float alpha = m.roughness * m.roughness;

    const vec3 H = normalize(V + L);

    // The min non-zero value avoids some artifacts and NaNs
    const float NdotL = max(dot(N, L), 1e-2);
    const float NdotV = max(dot(N, V), 1e-2);
    const float NdotH = max(dot(N, H), 0);
    const float LdotH = max(dot(L, H), 0);

    // Lambert diffuse, with the Disney Fresnel term
    const float F90 = pbr_f90(LdotH, alpha);
    const float Fd = pbr_fresnel_diffuse_disney(NdotL, F90) * pbr_fresnel_diffuse_disney(NdotV, F90);
    const float diffuse = NdotL * Fd * float_inv_pi();

    // Cook-Torrance for the specular part
    const float D = pbr_distribution_ggx(NdotH, alpha);
    const float G = pbr_shadowing_schlick_smith_ggx(NdotV, NdotL, alpha);
    const float F = pbr_fresnel_schlick(NdotV, m.ior);

    const float specular = (D * G * F) / (4 * NdotL * NdotV);

    return m.albedo * ((1 - m.metalness) * diffuse + specular);
}

#endif