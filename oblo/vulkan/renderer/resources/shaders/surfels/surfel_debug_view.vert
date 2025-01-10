#version 460

#extension GL_GOOGLE_include_directive : require

#include <renderer/camera>
#include <renderer/debug/printf>
#include <surfels/buffers/surfel_data_r>

layout(binding = 0) uniform b_CameraBuffer
{
    camera_buffer g_Camera;
};

layout(std430, binding = 1) restrict readonly buffer b_SphereGeometry
{
    float g_SphereGeometry[];
};

layout(location = 0) out vec3 out_SurfelPositionWS;

void main()
{
    const surfel_data surfel = g_SurfelData[gl_InstanceIndex];

    const uint vertexOffset = gl_VertexIndex * 3;
    const vec3 positionMS =
        vec3(g_SphereGeometry[vertexOffset], g_SphereGeometry[vertexOffset + 1], g_SphereGeometry[vertexOffset + 2]);

    const vec3 surfelPositionWS = surfel_data_world_position(surfel);
    const vec3 positionWS = positionMS * surfel.radius + surfelPositionWS;

    const vec4 positionNDC = g_Camera.viewProjection * vec4(positionWS, 1);

    gl_Position = positionNDC;
    out_SurfelPositionWS = surfelPositionWS;
}