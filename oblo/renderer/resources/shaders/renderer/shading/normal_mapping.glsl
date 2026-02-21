#ifndef OBLO_INCLUDE_RENDERER_SHADING_NORMAL_MAPPING
#define OBLO_INCLUDE_RENDERER_SHADING_NORMAL_MAPPING

#include <renderer/textures>

vec3 normal_map_apply(in vec4 sampledNormal, in vec3 T, in vec3 B, in vec3 N, in mat3 normalMatrix)
{
    const vec3 normalTS = 2.f * sampledNormal.xyz - 1.f;

    const mat3 TBN = mat3(T, B, N);

    return normalize(normalMatrix * (TBN * normalTS));
}

vec4 normal_map_sample_grad(in uint texture, in vec2 uv0, in vec2 uv0DDX, in vec2 uv0DDY)
{
    return texture_sample_2d_grad(texture, OBLO_SAMPLER_LINEAR_REPEAT, uv0, uv0DDX, uv0DDY);
}

#endif