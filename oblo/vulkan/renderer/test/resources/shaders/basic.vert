#version 450

layout(location = 0) in vec3 in_Position;

layout(location = 0) out vec3 out_Color;

void main()
{
    gl_Position = vec4(in_Position, 1.0);
    out_Color = vec3(1, 0, 0);
}