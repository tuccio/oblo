#ifndef OBLO_INCLUDE_RENDERER_MATERIAL
#define OBLO_INCLUDE_RENDERER_MATERIAL

struct gpu_material
{
    vec3 albedo;
    float metalness;
    float roughness;
    float emissive;
    uint albedoTexture;
    uint normalMapTexture;
    uint metalnessRoughnessTexture;
    uint emissiveTexture;
};

layout(buffer_reference) buffer i_MaterialBufferType
{
    gpu_material values[];
};

#endif