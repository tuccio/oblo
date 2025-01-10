#ifndef OBLO_INCLUDE_RENDERER_MESHES_MESH_INDICES
#define OBLO_INCLUDE_RENDERER_MESHES_MESH_INDICES

// These are required to use this header
// #extension GL_EXT_buffer_reference : require
// #extension GL_ARB_gpu_shader_int64 : require
// #extension GL_EXT_shader_16bit_storage : require

#include <renderer/buffer_reference/common>
#include <renderer/buffer_reference/u16>
#include <renderer/meshes/mesh_data>
#include <renderer/meshes/mesh_table>

// Index buffer fetch

uint16_t mesh_get_full_index_u16(in full_index_buffer fullIndexBuffer, in uint indexId)
{
    const uint64_t address = fullIndexBuffer.deviceAddress;
    U16AttributeType attributeBuffer = U16AttributeType(address);
    return attributeBuffer.values[indexId];
}

uint mesh_get_full_index_u32(in full_index_buffer fullIndexBuffer, in uint indexId)
{
    const uint64_t address = fullIndexBuffer.deviceAddress;
    U32AttributeType attributeBuffer = U32AttributeType(address);
    return attributeBuffer.values[indexId];
}

/// Returns the vertex indices for the specified triangle.
/// @remarks This is mostly useful in ray-tracing where we don't use micro-indices, but full index buffers.
uvec3 mesh_get_primitive_indices(in mesh_table t, in mesh_handle mesh, in uint primitiveId)
{
    const uint meshIndex = mesh_handle_as_index(mesh);
    const mesh_draw_range meshRange = mesh_get_draw_range(t, meshIndex);
    const full_index_buffer fullIndexBuffer = mesh_get_full_index_buffer(t, meshIndex);

    const uint verticesPerPrimitive = 3;

    // NOTE: We don't apply the mesh range to the index here, because we have a dedicated index buffer that we create in
    // draw_registry for the BLAS, the index buffer does not belong to the mesh_table currently
    const uint globalIndexOffset = primitiveId * verticesPerPrimitive;

    uvec3 localIndices;

    if (fullIndexBuffer.indexType == OBLO_MESH_DATA_INDEX_TYPE_U16)
    {
        localIndices[0] = uint(mesh_get_full_index_u16(fullIndexBuffer, globalIndexOffset));
        localIndices[1] = uint(mesh_get_full_index_u16(fullIndexBuffer, globalIndexOffset + 1));
        localIndices[2] = uint(mesh_get_full_index_u16(fullIndexBuffer, globalIndexOffset + 2));
    }
    else
    {
        localIndices[0] = mesh_get_full_index_u32(fullIndexBuffer, globalIndexOffset);
        localIndices[1] = mesh_get_full_index_u32(fullIndexBuffer, globalIndexOffset + 1);
        localIndices[2] = mesh_get_full_index_u32(fullIndexBuffer, globalIndexOffset + 2);
    }

    const uvec3 globalIndices = uvec3(meshRange.vertexOffset) + localIndices;
    return globalIndices;
}

#endif