#ifndef OBLO_INCLUDE_RENDERER_MESHES_MESH_DATA
#define OBLO_INCLUDE_RENDERER_MESHES_MESH_DATA

#include <renderer/geometry/volumes>
#include <renderer/meshes/mesh_table>

// These are required to use this header
// #extension GL_EXT_buffer_reference : require
// #extension GL_ARB_gpu_shader_int64 : require

// This needs to match mesh_draw_range in draw_registry
struct mesh_draw_range
{
    uint vertexOffset;
    uint indexOffset;
    uint meshletOffset;
    uint meshletCount;
};

// This needs to match mesh_table::meshlet_range
struct meshlet_draw_range
{
    uint vertexOffset;
    uint vertexCount;
    uint indexOffset;
    uint indexCount;
};

// Buffer references

layout(buffer_reference) buffer AabbAttributeType
{
    padded_aabb values[];
};

layout(buffer_reference) buffer MeshDrawRangeType
{
    mesh_draw_range values[];
};

layout(buffer_reference) buffer MeshletDrawRangeType
{
    meshlet_draw_range values[];
};

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

// Mesh draw ranges fetch

mesh_draw_range mesh_get_draw_range(in mesh_table t, in uint meshId)
{
    const uint64_t address = t.meshDataAddress + t.meshDataOffsets[OBLO_MESH_DATA_DRAW_RANGE];
    MeshDrawRangeType attributeBuffer = MeshDrawRangeType(address);
    return attributeBuffer.values[meshId];
}

meshlet_draw_range mesh_get_meshlet_draw_range(in mesh_table t, in uint meshletId)
{
    MeshletDrawRangeType attributeBuffer = MeshletDrawRangeType(t.meshletDataAddress);
    return attributeBuffer.values[meshletId];
}

#endif