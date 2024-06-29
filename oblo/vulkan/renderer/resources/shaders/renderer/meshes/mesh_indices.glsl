#ifndef OBLO_INCLUDE_RENDERER_MESHES_MESH_INDICES
#define OBLO_INCLUDE_RENDERER_MESHES_MESH_INDICES

// These are required to use this header
// #extension GL_EXT_buffer_reference : require
// #extension GL_ARB_gpu_shader_int64 : require
// #extension GL_EXT_shader_8bit_storage : require

#include <renderer/buffer_reference/u8>
#include <renderer/meshes/mesh_table>

// Index buffer fetch

uint8_t mesh_get_index_u8(in mesh_table t, in uint indexId)
{
    const uint64_t address = t.indexDataAddress;
    U8AttributeType attributeBuffer = U8AttributeType(address);
    return attributeBuffer.values[indexId];
}

#endif