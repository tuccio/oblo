#version 450 core

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

layout(push_constant) uniform c_PushConstants
{
    vec2 scale;
    vec2 translation;
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
    out_Data.uv = vec2(v.uv[0], v.uv[1]);
    gl_Position = vec4(vec2(v.pos[0], v.pos[1]) * g_Constants.scale + g_Constants.translation, 0, 1);
}