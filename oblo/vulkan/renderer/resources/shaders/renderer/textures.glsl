#ifndef OBLO_INCLUDE_RENDERER_TEXTURES
#define OBLO_INCLUDE_RENDERER_TEXTURES

#define OBLO_DESCRIPTOR_SET_SAMPLERS 1
#define OBLO_DESCRIPTOR_SET_TEXTURES_2D 2

#define OBLO_BINDING_SAMPLERS 32
#define OBLO_BINDING_TEXTURES_2D 33

layout(set = OBLO_DESCRIPTOR_SET_SAMPLERS, binding = OBLO_BINDING_SAMPLERS) uniform sampler g_Samplers[];
layout(set = OBLO_DESCRIPTOR_SET_TEXTURES_2D, binding = OBLO_BINDING_TEXTURES_2D) uniform texture2D g_Textures2D[];

#define OBLO_SAMPLER_LINEAR 0

uint get_texture_index(uint textureId)
{
    // We use 4 bits for generation id
    const uint generationBits = 4;
    const uint mask = ~0u >> generationBits;
    return textureId & mask;
}

vec4 sample_texture_2d(uint textureId, uint samplerId, vec2 uv)
{
    const uint textureIndex = get_texture_index(textureId);
    return texture(sampler2D(g_Textures2D[textureIndex], g_Samplers[samplerId]), uv);
}

#endif