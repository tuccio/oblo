#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include <renderer/textures>

layout(location = 0) in vec2 in_UV0;
layout(location = 1) flat in uint in_InstanceId;

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

void main()
{
    const gpu_material material = materials[in_InstanceId];

    const vec4 color = sample_texture_2d(material.albedoTexture, OBLO_SAMPLER_LINEAR, in_UV0);

    out_Color = vec4(color.xyz * material.albedo, 1);

#ifdef OBLO_PICKING_ENABLED
    out_PickingId = entityIds[in_InstanceId];
#endif
}