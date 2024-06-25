#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#extension GL_ARB_gpu_shader_int64 : require
#extension GL_EXT_shader_16bit_storage : require

#include <renderer/camera>
#include <renderer/instances>
#include <renderer/meshes>
#include <renderer/transform>
#include <visibility/visibility_buffer>

layout(location = 0) out uvec2 out_VisibilityBufferData;

layout(binding = 0) uniform b_CameraBuffer
{
    camera_buffer g_Camera;
};

layout(std430, binding = 2) restrict readonly buffer b_PreCullingIdMap
{
    uint g_preCullingIdMap[];
};

layout(push_constant) uniform c_PushConstants
{
    uint instanceTableId;
}
g_Constants;

void main()
{
    const uint preCullingId = g_preCullingIdMap[gl_DrawID];

    const mesh_handle meshHandle = OBLO_INSTANCE_DATA(g_Constants.instanceTableId, i_MeshHandles, preCullingId);
    const mesh_table table = mesh_table_fetch(meshHandle);

    const vec3 inPosition = mesh_get_position(table, gl_VertexIndex);

    const transform instanceTransform =
        OBLO_INSTANCE_DATA(g_Constants.instanceTableId, i_TransformBuffer, preCullingId);

    const mat4 localToWorld = instanceTransform.localToWorld;

    const vec4 positionWS = localToWorld * vec4(inPosition, 1);
    const vec4 positionNDC = g_Camera.viewProjection * positionWS;

    gl_Position = positionNDC;

    visibility_buffer_data visBufferData;
    visBufferData.instanceTableId = g_Constants.instanceTableId;
    visBufferData.instanceId = preCullingId;
    visBufferData.triangleIndex = mesh_get_triangle_index(table, gl_VertexIndex);

    out_VisibilityBufferData = visibility_buffer_pack(visBufferData);
}