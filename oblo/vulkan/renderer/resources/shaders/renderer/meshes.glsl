#ifndef OBLO_INCLUDE_RENDERER_MESHES
#define OBLO_INCLUDE_RENDERER_MESHES

#include <renderer/volumes>

// These are required to use this header
// #extension GL_EXT_buffer_reference : require
// #extension GL_ARB_gpu_shader_int64 : require
// #extension GL_EXT_shader_16bit_storage : require

// These need to match oblo::vertex_attributes in C++
#define OBLO_VERTEX_ATTRIBUTE_POSITION 0
#define OBLO_VERTEX_ATTRIBUTE_NORMAL 1
#define OBLO_VERTEX_ATTRIBUTE_UV0 2
#define OBLO_VERTEX_ATTRIBUTE_MAX 6

#define OBLO_MESH_DATA_DRAW_RANGE 0
#define OBLO_MESH_DATA_AABBS 1
#define OBLO_MESH_DATA_MAX 2

#define OBLO_DESCRIPTOR_SET_MESH_DATABASE 0
#define OBLO_BINDING_MESH_DATABASE 33

// These need to match oblo::mesh_index_type
#define OBLO_MESH_DATA_INDEX_TYPE_NONE 0
#define OBLO_MESH_DATA_INDEX_TYPE_U16 1
#define OBLO_MESH_DATA_INDEX_TYPE_U32 2

// This needs to match mesh_database::mesh_table_gpu
struct mesh_table
{
    uint64_t vertexDataAddress;
    uint64_t indexDataAddress;
    uint64_t meshDataAddress;
    uint attributesMask;
    uint indexType;
    uint attributeOffsets[OBLO_VERTEX_ATTRIBUTE_MAX];
    uint meshDataOffsets[OBLO_MESH_DATA_MAX];
};

layout(std430, binding = OBLO_BINDING_MESH_DATABASE) restrict readonly buffer b_MeshTables
{
    mesh_table g_MeshTables[];
};

struct mesh_handle
{
    uint value;
};

layout(buffer_reference) buffer i_MeshHandlesType
{
    mesh_handle values[];
};

uint mesh_get_table_index(in mesh_handle h)
{
    return h.value >> 24u;
}

uint mesh_get_index(in mesh_handle h)
{
    const uint mask = ~0u >> 8u;
    return (h.value & mask) - 1;
}

mesh_table mesh_table_fetch(in mesh_handle h)
{
    return g_MeshTables[mesh_get_table_index(h)];
}

struct vec3_attribute
{
    float x, y, z;
};

struct mesh_draw_range
{
    uint vertexOffset;
    uint vertexCount;
    uint indexOffset;
    uint indexCount;
};

// Buffer references

layout(buffer_reference) buffer Vec2AttributeType
{
    vec2 values[];
};

layout(buffer_reference) buffer Vec3AttributeType
{
    vec3_attribute values[];
};

layout(buffer_reference) buffer U16AttributeType
{
    uint16_t values[];
};

layout(buffer_reference) buffer U32AttributeType
{
    uint values[];
};

layout(buffer_reference) buffer AabbAttributeType
{
    padded_aabb values[];
};

layout(buffer_reference) buffer MeshDrawRangeType
{
    mesh_draw_range values[];
};

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

vec2 mesh_get_uv0(in mesh_table t, in uint vertexId)
{
    return mesh_get_vec2_attribute(t, OBLO_VERTEX_ATTRIBUTE_UV0, vertexId);
}

// Mesh data fetch
aabb mesh_get_aabb(in mesh_table t, in uint meshId)
{
    const uint64_t address = t.meshDataAddress + t.meshDataOffsets[OBLO_MESH_DATA_AABBS];
    AabbAttributeType attributeBuffer = AabbAttributeType(address);
    const padded_aabb padded = attributeBuffer.values[meshId];

    aabb res;
    res.min = padded.min;
    res.max = padded.max;
    return res;
}

mesh_draw_range mesh_get_draw_range(in mesh_table t, in uint meshId)
{
    const uint64_t address = t.meshDataAddress + t.meshDataOffsets[OBLO_MESH_DATA_DRAW_RANGE];
    MeshDrawRangeType attributeBuffer = MeshDrawRangeType(address);
    return attributeBuffer.values[meshId];
}

uint mesh_get_triangle_index(in mesh_table t, in uint vertexId)
{
    // This only really works with t.indexType == OBLO_MESH_DATA_INDEX_TYPE_NONE
    return vertexId / 3;
}

void mesh_get_vertex_indices(in mesh_table t, in uint triangleIndex, out uint vertexIndices[3])
{
    // This only really works with t.indexType == OBLO_MESH_DATA_INDEX_TYPE_NONE
    vertexIndices[0] = triangleIndex * 3;
    vertexIndices[1] = triangleIndex * 3 + 1;
    vertexIndices[2] = triangleIndex * 3 + 2;
}

#endif