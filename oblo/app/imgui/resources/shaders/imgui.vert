#version 450 core

struct imgui_vertex
{
    float posX;
    float posY;
    float uvS;
    float uvT;
    uint color;
};

layout(std430, binding = 0) restrict buffer readonly b_VertexData
{
    imgui_vertex g_VertexData[];
};

layout(push_constant) uniform c_PushConstants
{
    vec2 scale;
    vec2 translate;
}
g_Constants;

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(location = 0) out struct
{
    vec4 color;
    vec2 uv;
} out_Data;

void main()
{
    imgui_vertex v = g_VertexData[gl_VertexIndex];

    out_Data.color = unpackUnorm4x8(v.color);
    out_Data.uv = vec2(v.uvS, v.uvT);
    gl_Position = vec4(vec2(v.posX, v.posY) * g_Constants.scale + g_Constants.translate, 0, 1);
}