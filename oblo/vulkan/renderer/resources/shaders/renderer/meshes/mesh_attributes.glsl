#ifndef OBLO_INCLUDE_RENDERER_MESHES_MESH_ATTRIBUTES
#define OBLO_INCLUDE_RENDERER_MESHES_MESH_ATTRIBUTES

#include <renderer/buffer_reference/common>
#include <renderer/meshes/mesh_table>
#include <renderer/volumes>

// These are required to use this header
// #extension GL_EXT_buffer_reference : require
// #extension GL_ARB_gpu_shader_int64 : require

// Generic vertex data fetch

vec2 mesh_get_vec2_attribute(in mesh_table t, in uint attributeId, in uint vertexId)
{
    const uint64_t address = t.vertexDataAddress + t.attributeOffsets[attributeId];
    Vec2AttributeType attributeBuffer = Vec2AttributeType(address);
    return attributeBuffer.values[vertexId];
}

vec3 mesh_get_vec3_attribute(in mesh_table t, in uint attributeId, in uint vertexId)
{
    const uint64_t address = t.vertexDataAddress + t.attributeOffsets[attributeId];
    Vec3AttributeType attributeBuffer = Vec3AttributeType(address);
    const vec3_attribute a = attributeBuffer.values[vertexId];
    return vec3(a.x, a.y, a.z);
}

// Vertex attributes fetch

vec3 mesh_get_position(in mesh_table t, in uint vertexId)
{
    return mesh_get_vec3_attribute(t, OBLO_VERTEX_ATTRIBUTE_POSITION, vertexId);
}

vec3 mesh_get_normal(in mesh_table t, in uint vertexId)
{
    return mesh_get_vec3_attribute(t, OBLO_VERTEX_ATTRIBUTE_NORMAL, vertexId);
}

vec3 mesh_get_tangent(in mesh_table t, in uint vertexId)
{
    return mesh_get_vec3_attribute(t, OBLO_VERTEX_ATTRIBUTE_TANGENT, vertexId);
}

vec3 mesh_get_bitangent(in mesh_table t, in uint vertexId)
{
    return mesh_get_vec3_attribute(t, OBLO_VERTEX_ATTRIBUTE_BITANGENT, vertexId);
}

vec2 mesh_get_uv0(in mesh_table t, in uint vertexId)
{
    return mesh_get_vec2_attribute(t, OBLO_VERTEX_ATTRIBUTE_UV0, vertexId);
}

#endif