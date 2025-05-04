#ifndef OBLO_INCLUDE_RENDERER_TEXTURES
#define OBLO_INCLUDE_RENDERER_TEXTURES

// This is required to use this header
// #extension GL_EXT_nonuniform_qualifier : require

#define OBLO_DESCRIPTOR_SET_SAMPLERS 1
#define OBLO_DESCRIPTOR_SET_TEXTURES_2D 2

#define OBLO_BINDING_SAMPLERS 32
#define OBLO_BINDING_TEXTURES_2D 33

layout(set = OBLO_DESCRIPTOR_SET_SAMPLERS, binding = OBLO_BINDING_SAMPLERS) uniform sampler g_Samplers[];
layout(set = OBLO_DESCRIPTOR_SET_TEXTURES_2D, binding = OBLO_BINDING_TEXTURES_2D) uniform texture2D g_Textures2D[];

// These need to match the sampler enum in C++
#define OBLO_SAMPLER_LINEAR_REPEAT 0
#define OBLO_SAMPLER_LINEAR_CLAMP_BLACK 1
#define OBLO_SAMPLER_LINEAR_CLAMP_EDGE 2
#define OBLO_SAMPLER_NEAREST 3
#define OBLO_SAMPLER_ANISOTROPIC 4

uint texture_get_index(uint textureId)
{
    // We use 4 bits for generation id
    const uint generationBits = 4;
    const uint mask = ~0u >> generationBits;
    return textureId & mask;
}

vec4 texture_sample_2d(uint textureId, uint samplerId, vec2 uv)
{
    const uint textureIndex = texture_get_index(textureId);
    return texture(sampler2D(g_Textures2D[textureIndex], g_Samplers[samplerId]), uv);
}

vec4 texture_sample_2d_lod(uint textureId, uint samplerId, vec2 uv, float lod)
{
    const uint textureIndex = texture_get_index(textureId);
    return textureLod(sampler2D(g_Textures2D[textureIndex], g_Samplers[samplerId]), uv, lod);
}

vec4 texture_sample_2d_grad(uint textureId, uint samplerId, vec2 uv, vec2 uvDDX, vec2 uvDDY)
{
    const uint textureIndex = texture_get_index(textureId);
    return textureGrad(sampler2D(g_Textures2D[textureIndex], g_Samplers[samplerId]), uv, uvDDX, uvDDY);
}

int texture_query_levels(uint textureId, uint samplerId)
{
    const uint textureIndex = texture_get_index(textureId);
    return textureQueryLevels(sampler2D(g_Textures2D[textureIndex], g_Samplers[samplerId]));
}

#endif