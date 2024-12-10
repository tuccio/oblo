#version 460

#extension GL_GOOGLE_include_directive : require

#include <renderer/camera>
#include <renderer/debug/printf>
#include <surfels/buffers/surfel_pool_r>

layout(binding = 0) uniform b_CameraBuffer
{
    camera_buffer g_Camera;
};

void main()
{
    const surfel_data surfel = g_SurfelData[gl_InstanceIndex];

    const float scale = surfel.alive ? 0.01f : 0.f;

    // 1x1 quad centered around 0
    const vec3 positions[4] =
        vec3[4](vec3(.5f, .5f, 0.0f), vec3(-.5f, .5f, 0.0f), vec3(-.5f, -.5f, 0.0f), vec3(.5f, -.5f, 0.0f));

    const vec3 positionWS = positions[gl_VertexIndex] * scale + surfel.position;

    const vec4 positionNDC = g_Camera.viewProjection * vec4(positionWS, 1);

    gl_Position = positionNDC;
}