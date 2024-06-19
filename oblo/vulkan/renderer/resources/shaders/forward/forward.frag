#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_buffer_reference : require
#extension GL_ARB_gpu_shader_int64 : require

#include <renderer/instances>
#include <renderer/lights>
#include <renderer/material>
#include <renderer/textures>
#include <renderer/volumes>

layout(location = 0) flat in uint in_InstanceId;
layout(location = 1) in vec3 in_PositionWS;
layout(location = 2) in vec3 in_Normal;
layout(location = 3) in vec2 in_UV0;

layout(location = 0) out vec4 out_Color;
layout(location = 1) out uint out_PickingId;

layout(buffer_reference) buffer i_EntityIdBufferType
{
    uint values[];
};

layout(binding = 3) uniform b_LightConfig
{
    light_config g_LightConfig;
};

layout(std430, binding = 4) restrict readonly buffer b_LightData
{
    light_data lights[];
};

layout(push_constant) uniform c_PushConstants
{
    uint instanceTableId;
}
g_Constants;

void main()
{
    const gpu_material material = OBLO_INSTANCE_DATA(g_Constants.instanceTableId, i_MaterialBuffer, in_InstanceId);

    const vec4 color = texture_sample_2d(material.albedoTexture, OBLO_SAMPLER_LINEAR, in_UV0);

    vec3 reflected = vec3(0);

    for (uint lightIndex = 0; lightIndex < g_LightConfig.lightsCount; ++lightIndex)
    {
        reflected += light_contribution(lights[lightIndex], in_PositionWS, in_Normal);
    }

    out_Color = vec4(color.xyz * material.albedo * reflected, 1);

#ifdef OBLO_PICKING_ENABLED
    const uint entityId = OBLO_INSTANCE_DATA(g_Constants.instanceTableId, i_EntityIdBuffer, in_InstanceId);
    out_PickingId = entityId;
#endif
}