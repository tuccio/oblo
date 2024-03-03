#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#extension GL_ARB_gpu_shader_int64 : require

#include <renderer/camera>
#include <renderer/meshes>
#include <renderer/transform>

layout(location = 0) out vec2 out_UV0;
layout(location = 1) out uint out_InstanceId;

layout(binding = 0) uniform b_CameraBuffer
{
    camera_buffer g_Camera;
};

layout(std430, binding = 1) restrict readonly buffer i_TransformBuffer
{
    transform g_Transforms[];
};

void main()
{
    const mesh_handle meshHandle = g_MeshHandles[gl_DrawID];
    const mesh_table table = get_mesh_table(meshHandle);

    const vec3 inPosition = get_mesh_position(table, gl_VertexIndex);
    const vec2 inUV0 = get_mesh_uv0(table, gl_VertexIndex);

    const mat4 model = g_Transforms[gl_DrawID].localToWorld;
    const mat4 viewProj = g_Camera.projection * g_Camera.view;
    const vec4 position = viewProj * model * vec4(inPosition, 1);
    gl_Position = position;
    out_UV0 = inUV0;
    out_InstanceId = gl_DrawID;
}