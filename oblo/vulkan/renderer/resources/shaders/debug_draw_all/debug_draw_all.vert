#version 460

layout(location = 0) in vec3 in_Position;

layout(binding = 0) uniform CameraBuffer
{
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
}
b_Camera;

struct transform
{
    mat4 localToWorld;
};

layout(std430, binding = 1) restrict readonly buffer i_TransformBuffer
{
    transform transforms[];
};

void main()
{
    const mat4 model = transforms[gl_DrawID].localToWorld;
    const mat4 viewProj =  b_Camera.projection * b_Camera.view;
    gl_Position = viewProj * model * vec4(in_Position, 1);
}