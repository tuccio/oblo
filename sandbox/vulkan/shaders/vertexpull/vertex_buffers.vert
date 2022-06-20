#version 450

layout(location = 0) in vec3 in_Position;
layout(location = 1) in vec3 in_Color;

layout(location = 0) out vec3 out_Color;

struct transform_data
{
    vec3 translation;
    float scale;
};

layout(std430, binding = 0) buffer b_Transform
{
    transform_data in_Transform[];
};

void main()
{
    const transform_data transform = in_Transform[gl_InstanceIndex];
    gl_Position = vec4(in_Position * transform.scale + transform.translation, 1.0);
    out_Color = in_Color;
}