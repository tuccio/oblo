#version 450

layout(location = 0) in vec3 in_Position;

layout(binding = 0) uniform CameraBuffer
{
    mat4 viewProjection;
}
b_Camera;

void main()
{
#if 0
    vec3 position = in_Position;

    position.z /= 100.f;
    position.z += 0.2f;

    gl_Position = vec4(position, 1);
#else
    gl_Position = b_Camera.viewProjection * vec4(in_Position, 1);
#endif
}