#ifndef OBLO_INCLUDE_RENDERER_SHADING_PBR_UTILITY
#define OBLO_INCLUDE_RENDERER_SHADING_PBR_UTILITY

#include <renderer/material>
#include <renderer/shading/pbr>
#include <renderer/textures>

pbr_material pbr_extract_parameters(in gpu_material material, in vec2 uv0, in vec2 uv0DDX, in vec2 uv0DDY)
{
    // Extract the PBR parameters from the material
    pbr_material pbr;
    pbr.albedo = material.albedo;
    pbr.metalness = material.metalness;
    pbr.roughness = material.roughness;
    pbr.ior = material.ior;

    if (material.albedoTexture != 0)
    {
        const vec3 albedo =
            texture_sample_2d_grad(material.albedoTexture, OBLO_SAMPLER_ANISOTROPIC, uv0, uv0DDX, uv0DDY).xyz;

        pbr.albedo *= albedo;
    }

    if (material.metalnessRoughnessTexture != 0)
    {
        const vec4 metalnessRoughness =
            texture_sample_2d_grad(material.metalnessRoughnessTexture, OBLO_SAMPLER_ANISOTROPIC, uv0, uv0DDX, uv0DDY);

        pbr.metalness *= metalnessRoughness.x;
        pbr.roughness *= metalnessRoughness.y;
    }

    return pbr;
}

#endif