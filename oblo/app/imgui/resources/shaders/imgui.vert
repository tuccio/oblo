#version 450 core

#include <renderer/math>

#include <imgui_constants>

struct imgui_vertex
{
    float pos[2];
    float uv[2];
    uint color;
};

layout(std430, binding = 0) restrict buffer readonly b_VertexData
{
    imgui_vertex g_VertexData[];
};

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(location = 0) out struct
{
    vec4 color;
    vec2 uv;
} out_Data;

vec4 srgb_to_linear(uint srgb)
{
    const float gamma = 2.2f;

    const vec4 color = unpackUnorm4x8(srgb);
    return vec4(pow(color.xyz, gamma), color.w);
}

void main()
{
    imgui_vertex v = g_VertexData[gl_VertexIndex];

    const vec2 position = vec2(v.pos[0], v.pos[1]);
    const vec2 uv = vec2(v.uv[0], v.uv[1]);
    const vec4 color = srgb_to_linear(v.color);

    out_Data.color = color;
    out_Data.uv = uv;

    gl_Position = vec4(position * g_Constants.scale + g_Constants.translation, 0, 1);
}