#ifndef OBLO_INCLUDE_RENDERER_SHADING_NORMAL_MAPPING
#define OBLO_INCLUDE_RENDERER_SHADING_NORMAL_MAPPING

vec3 normal_mapping(in vec4 sampledNormal, in vec3 T, in vec3 B, in vec3 N, in mat3 normalMatrix)
{
    const vec3 normalTS = 2.f * sampledNormal.xyz - 1.f;

    const mat3 TBN = mat3(T, B, N);

    return normalize(normalMatrix * (TBN * normalTS));
}

#endif