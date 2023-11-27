#version 460

layout(location = 0) in vec3 in_Position;
// layout(location = 1) in vec3 in_UV0;

layout(location = 0) out vec2 out_UV0;
layout(location = 1) out uint out_InstanceId;


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
    const vec4 position = viewProj * model * vec4(in_Position, 1);
    gl_Position = position;
    out_UV0 = position.xy;
}