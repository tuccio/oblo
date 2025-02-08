#version 450 core

layout(location = 0) out vec4 out_Color;
// layout(set = 0, binding = 1) uniform sampler2D s_Texture;

layout(location = 0) in struct
{
    vec4 color;
    vec2 uv;
} in_Data;

void main()
{
    out_Color = in_Data.color;
    // out_Color = in_Data.color * texture(s_Texture, in_Data.uv.st);
}