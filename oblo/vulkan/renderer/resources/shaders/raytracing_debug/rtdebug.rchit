#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_buffer_reference : require
#extension GL_ARB_gpu_shader_int64 : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_shader_16bit_storage : require
#extension GL_EXT_control_flow_attributes : require

#include <renderer/camera>
#include <renderer/geometry/barycentric>
#include <renderer/instance_id>
#include <renderer/instances>
#include <renderer/lights>
#include <renderer/material>
#include <renderer/meshes/mesh_attributes>
#include <renderer/meshes/mesh_data>
#include <renderer/meshes/mesh_indices_rt>
#include <renderer/meshes/mesh_table>
#include <renderer/shading/pbr_utility>
#include <renderer/textures>

layout(binding = 16) uniform b_CameraBuffer
{
    camera_buffer g_Camera;
};

layout(binding = 0) uniform b_LightConfig
{
    light_config g_LightConfig;
};

layout(std430, binding = 1) restrict readonly buffer b_LightData
{
    light_data g_Lights[];
};

layout(location = 0) rayPayloadInEXT vec3 r_HitColor;

hitAttributeEXT vec2 h_BarycentricCoords;

void main()
{
    const uint globalInstanceId = gl_InstanceCustomIndexEXT;

    uint instanceTableId;
    uint instanceId;

    instance_parse_global_id(globalInstanceId, instanceTableId, instanceId);

    const mesh_handle mesh = OBLO_INSTANCE_DATA(instanceTableId, i_MeshHandles, instanceId);
    const gpu_material material = OBLO_INSTANCE_DATA(instanceTableId, i_MaterialBuffer, instanceId);

    barycentric_coords bc;
    bc.lambda = vec3(1.f - h_BarycentricCoords.x - h_BarycentricCoords.y, h_BarycentricCoords.x, h_BarycentricCoords.y);

    // r_HitColor = debug_color_map(uint(gl_InstanceCustomIndexEXT));
    vec2 triangleUV0[3];
    vec3 trianglePosition[3];
    vec3 triangleNormal[3];

    // Read the mesh data
    const mesh_table meshTable = mesh_table_fetch(mesh);

    const uvec3 vertexIndices = mesh_get_primitive_indices(meshTable, mesh, gl_PrimitiveID);

    [[unroll]] for (uint i = 0; i < 3; ++i)
    {
        const uint vertexId = vertexIndices[i];
        trianglePosition[i] = mesh_get_position(meshTable, vertexId);
        triangleNormal[i] = mesh_get_normal(meshTable, vertexId);
        triangleUV0[i] = mesh_get_uv0(meshTable, vertexId);
    }

    const vec3 position = barycentric_interpolate(bc, trianglePosition);
    const vec3 normal = barycentric_interpolate(bc, triangleNormal);
    const vec2 uv0 = barycentric_interpolate(bc, triangleUV0);

    const vec3 positionWS = vec3(gl_ObjectToWorldEXT * vec4(position, 1.f));
    const vec3 normalWS = normalize(vec3(normal * gl_WorldToObjectEXT));

    // TODO: Calculate the derivatives
    const vec2 uv0DDX = vec2(0);
    const vec2 uv0DDY = vec2(0);

    const pbr_material pbr = pbr_extract_parameters(material, uv0, uv0DDX, uv0DDY);

    vec3 reflected = vec3(0);
    const vec3 viewWS = normalize(g_Camera.position - positionWS);

    for (uint lightIndex = 0; lightIndex < g_LightConfig.lightsCount; ++lightIndex)
    {
        vec3 L;

        const vec3 contribution = light_contribution(g_Lights[lightIndex], positionWS, L);
        const vec3 brdf = pbr_brdf(normalWS, viewWS, L, pbr);

        reflected += contribution * brdf;
    }

    r_HitColor = reflected;
}