#ifndef OBLO_INCLUDE_RENDERER_MATERIAL
#define OBLO_INCLUDE_RENDERER_MATERIAL

struct gpu_material
{
    vec3 albedo;
    uint albedoTexture;
};

layout(buffer_reference) buffer i_MaterialBufferType
{
    gpu_material values[];
};

#endif