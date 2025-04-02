#ifndef OBLO_INCLUDE_TONE_MAPPING_TONEMAP
#define OBLO_INCLUDE_TONE_MAPPING_TONEMAP

// Simple tone mapping from "Photographic Tone Reproduction for Digital Images"
vec3 tonemap_reinhard_eq3(in vec3 x)
{
    return x / (x + 1);
}

// ACES approximation from https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
vec3 tonemap_aces_approx(in vec3 x)
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

const mat3 g_ACESInputMat = {
    {0.59719, 0.07600, 0.02840},
    {0.35458, 0.90834, 0.13383},
    {0.04823, 0.01566, 0.83777},
};

// ODT_SAT => XYZ => D60_2_D65 => sRGB
const mat3 g_ACESOutputMat = {
    {1.60475, -0.10208, -0.00327},
    {-0.53108, 1.10813, -0.07276},
    {-0.07367, -0.00605, 1.07602},
};

vec3 tonemap_aces_hill_rrt_odt_fit(in vec3 v)
{
    const vec3 a = v * (v + 0.0245786f) - 0.000090537f;
    const vec3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
    return a / b;
}

// ACES fit adapted from https://github.com/TheRealMJP/BakingLab/blob/master/BakingLab/ACES.hlsl
vec3 tonemap_aces_hill(in vec3 x)
{
    vec3 color = g_ACESInputMat * x;

    // Apply RRT and ODT
    color = tonemap_aces_hill_rrt_odt_fit(color);

    color = g_ACESOutputMat * color;

    // Clamp to [0, 1]
    color = saturate(color);

    return color;
}

#endif