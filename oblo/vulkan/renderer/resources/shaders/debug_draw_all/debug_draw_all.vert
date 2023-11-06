#version 450

layout(location = 0) in vec3 in_Position;

layout(binding = 0) uniform CameraBuffer
{
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
}
b_Camera;

void main()
{
    const mat4 viewProj =  b_Camera.projection * b_Camera.view;
    gl_Position = viewProj * vec4(in_Position, 1);
}