#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#extension GL_ARB_gpu_shader_int64 : require

#include <renderer/camera>
#include <renderer/instances>
#include <renderer/meshes>
#include <renderer/transform>

layout(location = 0) out uint out_InstanceId;
layout(location = 1) out vec3 out_PositionWS;
layout(location = 2) out vec3 out_Normal;
layout(location = 3) out vec2 out_UV0;

layout(binding = 0) uniform b_CameraBuffer
{
    camera_buffer g_Camera;
};

layout(push_constant) uniform c_PushConstants
{
    uint instanceTableId;
}
g_Constants;





void main()
{
    const mesh_handle meshHandle = OBLO_INSTANCE_DATA(g_Constants.instanceTableId, i_MeshHandles, gl_DrawID);
    const mesh_table table = get_mesh_table(meshHandle);

    const vec3 inPosition = get_mesh_position(table, gl_VertexIndex);
    const vec2 inUV0 = get_mesh_uv0(table, gl_VertexIndex);
    const vec3 inNormal = get_mesh_normal(table, gl_VertexIndex);

    const transform instanceTransform = OBLO_INSTANCE_DATA(g_Constants.instanceTableId, i_TransformBuffer, gl_DrawID);
    const mat4 model = instanceTransform.localToWorld;

    const mat4 viewProj = g_Camera.projection * g_Camera.view;

    const vec4 positionWS = model * vec4(inPosition, 1);
    const vec4 positionNDC = viewProj * positionWS;

    gl_Position = positionNDC;

    out_PositionWS = positionWS.xyz;
    out_InstanceId = gl_DrawID;
    out_Normal = inNormal;
    out_UV0 = inUV0;
}