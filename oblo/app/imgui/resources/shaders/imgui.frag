#version 450 core

#extension GL_EXT_nonuniform_qualifier : require

#include <imgui_constants>
#include <renderer/color>
#include <renderer/textures>

layout(location = 0) out vec4 out_Color;

layout(location = 0) in struct
{
    vec4 color;
    vec2 uv;
} in_Data;

void main()
{
    vec4 color = vec4(linear_to_srgb_float(in_Data.color.xyz), in_Data.color.a);

    if (g_Constants.textureId != 0)
    {
        // Do we need to color correct this? We might sample unorm render targets, which ar actually sRGB
        color *= texture_sample_2d(g_Constants.textureId, OBLO_SAMPLER_LINEAR, in_Data.uv);
    }

    out_Color = color;
}