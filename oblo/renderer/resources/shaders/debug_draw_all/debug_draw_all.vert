#version 460

layout(location = 0) in vec3 in_Position;

layout(location = 0) out vec3 out_Color;

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

float random(float seedX, float seedY)
{
    const vec2 seed = vec2(seedX, seedY);
    return fract(sin(dot(seed, vec2(21.41f, 13.2f)) * 4.f) * 10.f);
}

void main()
{
    const mat4 model = transforms[gl_DrawID].localToWorld;
    const mat4 viewProj = b_Camera.projection * b_Camera.view;
    const vec4 position = viewProj * model * vec4(in_Position, 1);
    gl_Position = position;
    out_Color = vec3(random(gl_DrawID, 0), random(gl_DrawID, 1), random(gl_DrawID, 2));
}