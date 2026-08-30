#version 450 core

#include <renderer/math>

struct ui_instance
{
    vec4 rect;
    vec4 color;
    vec4 cornerRadius;
    vec4 uvRect;
    uint textureId;
    uint _pad0;
    uint _pad1;
    uint _pad2;
};

layout(std430, binding = 0) restrict buffer readonly b_ElementsData
{
    ui_instance g_Instances[];
};

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(location = 0) out struct
{
    vec4 color;
    vec4 cornerRadius;
    vec2 position;
    vec2 halfSize;
    vec2 uv;
} out_Data;

layout(location = 5) out uint out_TextureID;

layout(push_constant) uniform PushConstants
{
    uvec2 g_Resolution;
};

void main()
{
    const ui_instance e = g_Instances[gl_InstanceIndex];

    const vec2 offsets[4] = vec2[](vec2(0), vec2(0, 1), vec2(1, 1), vec2(1, 0));
    const vec2 position = e.rect.xy + offsets[gl_VertexIndex] * e.rect.zw;

    const vec2 ndc = position / vec2(g_Resolution) * 2.0 - 1.0;

    out_Data.color = e.color;
    out_Data.cornerRadius = e.cornerRadius;
    out_Data.position = position - (e.rect.xy + e.rect.zw * 0.5);
    out_Data.halfSize = e.rect.zw * 0.5;
    out_Data.uv = e.uvRect.xy + offsets[gl_VertexIndex] * e.uvRect.zw;
    out_TextureID = e.textureId;

    gl_Position = vec4(ndc, 0, 1);
}
