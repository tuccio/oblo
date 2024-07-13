#ifndef OBLO_INCLUDE_RENDERER_MESHES_MESH_INDICES
#define OBLO_INCLUDE_RENDERER_MESHES_MESH_INDICES

// These are required to use this header
// #extension GL_EXT_buffer_reference : require
// #extension GL_ARB_gpu_shader_int64 : require
// #extension GL_EXT_shader_8bit_storage : require

#include <renderer/buffer_reference/u8>
#include <renderer/meshes/mesh_data>
#include <renderer/meshes/mesh_table>

// Index buffer fetch

uint8_t mesh_get_index_u8(in mesh_table t, in uint indexId)
{
    const uint64_t address = t.indexDataAddress;
    U8AttributeType attributeBuffer = U8AttributeType(address);
    return attributeBuffer.values[indexId];
}

/// Returns the meshlet microindices.
/// @remarks The vertices have to be translated to be used to access the data globally.
uvec3 meshlet_get_triangle_microindices(
    in mesh_table t, in mesh_draw_range meshRange, in meshlet_draw_range meshletRange, in uint meshletTriangleId)
{
    const uint64_t address = t.indexDataAddress;
    U8AttributeType indexBuffer = U8AttributeType(address);

    const uint localIndexOffset = meshletTriangleId * 3;
    const uint globalIndexOffset = meshRange.indexOffset + meshletRange.indexOffset + localIndexOffset;

    uvec3 triangleIndices;
    triangleIndices[0] = uint(mesh_get_index_u8(t, globalIndexOffset));
    triangleIndices[1] = uint(mesh_get_index_u8(t, globalIndexOffset + 1));
    triangleIndices[2] = uint(mesh_get_index_u8(t, globalIndexOffset + 2));

    return triangleIndices;
}

/// Returns the meshlet microindices for the meshlet.
uvec3 mesh_get_meshlet_indices(in mesh_table t, in mesh_handle mesh, in uint meshletId, in uint meshletTriangleId)
{
    const uint meshIndex = mesh_handle_as_index(mesh);
    const mesh_draw_range meshRange = mesh_get_draw_range(t, meshIndex);

    const meshlet_draw_range meshletRange = mesh_get_meshlet_draw_range(t, meshletId);

    const uvec3 vertexIndices = uvec3(meshRange.vertexOffset + meshletRange.vertexOffset) +
        meshlet_get_triangle_microindices(t, meshRange, meshletRange, meshletTriangleId);

    return vertexIndices;
}

#endif