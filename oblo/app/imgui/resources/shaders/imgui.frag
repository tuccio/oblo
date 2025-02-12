#version 450 core

#extension GL_EXT_nonuniform_qualifier : require

#include <imgui_constants>
#include <renderer/textures>

layout(location = 0) out vec4 out_Color;

layout(location = 0) in struct
{
    vec4 color;
    vec2 uv;
} in_Data;

void main()
{
    out_Color = in_Data.color;

    if (g_Constants.textureId != 0)
    {
        out_Color *= texture_sample_2d(g_Constants.textureId, OBLO_SAMPLER_LINEAR, in_Data.uv);
    }
}