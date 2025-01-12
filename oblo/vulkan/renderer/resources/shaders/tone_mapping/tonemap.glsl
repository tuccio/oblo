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

#endif