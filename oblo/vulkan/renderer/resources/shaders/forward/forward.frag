#version 460

#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec2 in_UV0;
layout(location = 1) flat in uint in_InstanceId;

layout(location = 0) out vec4 out_Color;
layout(location = 1) out uint out_PickingId;

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

struct gpu_material
{
    vec3 albedo;
    uint albedoTexture;
};

#ifdef OBLO_PICKING_ENABLED

layout(binding = 10) uniform PickingCoordinatesBuffer
{
    vec2 coordinates;
}
b_Picking;

layout(std430, binding = 11) restrict buffer b_PickingResult
{
    uint pickingId;
};

layout(std430, binding = 12) restrict readonly buffer i_EntityIdBuffer
{
    uint entityIds[];
};


#endif

layout(std430, binding = 2) restrict readonly buffer i_MaterialBuffer
{
    gpu_material materials[];
};

void main()
{
    const gpu_material material = materials[in_InstanceId];

    const vec4 color = sample_texture_2d(material.albedoTexture, OBLO_SAMPLER_LINEAR, in_UV0);

    out_Color = vec4(color.xyz * material.albedo, 1);

#ifdef OBLO_PICKING_ENABLED
    const uvec2 uFrag = uvec2(gl_FragCoord);
    const uvec2 uCoords = uvec2(b_Picking.coordinates);

    if (uFrag.x == uCoords.x && uFrag.y == uCoords.y)
    { 
        pickingId = entityIds[in_InstanceId];
    }
    
    out_PickingId = entityIds[in_InstanceId];
#endif
}