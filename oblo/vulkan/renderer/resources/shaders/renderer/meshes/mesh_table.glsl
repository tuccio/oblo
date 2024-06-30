#ifndef OBLO_INCLUDE_RENDERER_MESHES_MESH_TABLE
#define OBLO_INCLUDE_RENDERER_MESHES_MESH_TABLE

#include <renderer/meshes/mesh_defines>

// These are required to use this header
// #extension GL_EXT_buffer_reference : require
// #extension GL_ARB_gpu_shader_int64 : require

// This needs to match mesh_database::mesh_table_gpu
struct mesh_table
{
    uint64_t vertexDataAddress;
    uint64_t indexDataAddress;
    uint64_t meshDataAddress;
    uint64_t meshletDataAddress;
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

uint mesh_handle_as_table_index(in mesh_handle h)
{
    return h.value >> 24u;
}

uint mesh_handle_as_index(in mesh_handle h)
{
    const uint mask = ~0u >> 8u;
    return (h.value & mask) - 1;
}

mesh_table mesh_table_fetch(in mesh_handle h)
{
    return g_MeshTables[mesh_handle_as_table_index(h)];
}

#endif