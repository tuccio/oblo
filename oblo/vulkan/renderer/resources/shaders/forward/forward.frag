#version 460

#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec2 in_UV0;
layout(location = 1) flat in uint in_InstanceId;
layout(location = 0) out vec4 out_Color;

#define OBLO_DESCRIPTOR_SET_TEXTURES 1

#define OBLO_BINDING_SAMPLERS 32
#define OBLO_BINDING_TEXTURES2D 33

layout(set = OBLO_DESCRIPTOR_SET_TEXTURES, binding = OBLO_BINDING_SAMPLERS) uniform sampler g_Samplers[];
layout(set = OBLO_DESCRIPTOR_SET_TEXTURES, binding = OBLO_BINDING_TEXTURES2D) uniform texture2D g_Textures2D[];

struct gpu_material
{
    vec3 albedo;
    uint albedoTexture;
};

layout(std430, binding = 2) restrict readonly buffer i_MaterialBuffer
{
    gpu_material materials[];
};

void main()
{
    const gpu_material material = materials[in_InstanceId];

    const vec4 color = texture(sampler2D(g_Textures2D[material.albedoTexture], g_Samplers[0]), in_UV0);

    out_Color = vec4(material.albedo, 1);
}