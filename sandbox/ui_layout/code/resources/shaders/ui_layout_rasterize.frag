#version 450 core

#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) out vec4 out_Color;

layout(location = 0) in struct
{
    vec4 color;
    float cornerRadius;
    vec2 position;
    vec2 halfSize;
} in_Data;

float rounded_box_signed_distance(in vec2 p, in vec2 halfSize, in float r)
{
    const vec2 q = abs(p) - halfSize + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0) - r;
}

void main()
{
    const float d = rounded_box_signed_distance(in_Data.position, in_Data.halfSize, in_Data.cornerRadius);

    if (d > 0)
    {
        discard;
    }

    out_Color = in_Data.color;
}