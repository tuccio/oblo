#ifndef OBLO_INCLUDE_RENDERER_MATERIAL
#define OBLO_INCLUDE_RENDERER_MATERIAL

struct gpu_material
{
    vec3 albedo;
    uint albedoTexture;
    float metalness;
    float roughness;
    uint metalnessRoughnessTexture;
    uint normalMapTexture;
    float ior;
    uint _padding[3];
    vec3 emissive;
    uint emissiveTexture;
};

layout(buffer_reference) buffer i_MaterialBufferType
{
    gpu_material values[];
};

#endif