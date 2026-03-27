#ifndef OBLO_INCLUDE_RENDERER_MESHES_SKINNING
#define OBLO_INCLUDE_RENDERER_MESHES_SKINNING

// #extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

#include <ecs/entity>
#include <ecs/entity_set>
#include <renderer/buffer_reference/u16>
#include <renderer/debug/printf>
#include <renderer/instance_id>
#include <renderer/instances>
#include <renderer/meshes/mesh_attributes>
#include <renderer/meshes/mesh_table>

uvec4 mesh_get_u16vec4_attribute_as_uvec4(in mesh_table t, in uint attributeId, in uint vertexId)
{
    const uint64_t address = t.vertexDataAddress + t.attributeOffsets[attributeId];
    U16Vec4AttributeType attributeBuffer = U16Vec4AttributeType(address);
    const u16vec4 r = attributeBuffer.values[vertexId];
    return uvec4(r.x, r.y, r.z, r.w);
}

uvec4 mesh_get_joint_indices(in mesh_table t, in uint vertexId)
{
    return mesh_get_u16vec4_attribute_as_uvec4(t, OBLO_VERTEX_ATTRIBUTE_JOINT_INDICES, vertexId);
}

vec4 mesh_get_joint_weights(in mesh_table t, in uint vertexId)
{
    return mesh_get_vec4_attribute(t, OBLO_VERTEX_ATTRIBUTE_JOINT_WEIGHTS, vertexId);
}

// This needs to match joint_skinning_transform_chunks_component::max_chunks
const uint g_skinningMaxChunks = 31;

// This needs to match joint_skinning_transform_component::joints_per_chunk
const uint g_skinningJointsPerChunk = 16;

struct skinning_joint_chunks
{
    ecs_entity chunks[g_skinningMaxChunks];
    uint numJoints;
};

struct skinning_joint_transform
{
    mat4 jointMatrices[g_skinningJointsPerChunk];
};

layout(buffer_reference) buffer i_JointSkinningChunksBufferType
{
    skinning_joint_chunks values[];
};

layout(buffer_reference) buffer i_JointSkinningTransformBufferType
{
    skinning_joint_transform values[];
};

void skinning_split_chunk_index(in uint jointIndex, out uint chunkIndex, out uint localJointIndex)
{
    chunkIndex = jointIndex / g_skinningJointsPerChunk;
    localJointIndex = jointIndex % g_skinningJointsPerChunk;
}

mat4 skinning_fetch_matrix(in skinning_joint_chunks chunks, in uint jointIndex)
{
    uint chunkIndex, localJointIndex;
    skinning_split_chunk_index(jointIndex, chunkIndex, localJointIndex);

    mat4 result = mat4(0);

    if (chunkIndex < g_skinningMaxChunks && localJointIndex < g_skinningJointsPerChunk)
    {
        const ecs_entity jointChunk = chunks.chunks[chunkIndex];

        if (ecs_entity_is_valid(jointChunk))
        {
            ecs_entity_set_entry entityInfo;

            if (ecs_entity_set_try_find(jointChunk, entityInfo))
            {
                uint instanceTableId;
                uint instanceId;

                instance_parse_global_id(entityInfo.globalInstanceId, instanceTableId, instanceId);

                const skinning_joint_transform transform =
                    OBLO_INSTANCE_DATA(instanceTableId, i_JointSkinningTransformBuffer, instanceId);

                result = transform.jointMatrices[localJointIndex];
            }
        }
    }

    return result;
}

mat4 skinning_calculate_weighted_matrix(in skinning_joint_chunks chunks, in uvec4 jointIndices, in vec4 jointWeights)
{
    return jointWeights[0] * skinning_fetch_matrix(chunks, jointIndices[0]) +
        jointWeights[1] * skinning_fetch_matrix(chunks, jointIndices[1]) +
        jointWeights[2] * skinning_fetch_matrix(chunks, jointIndices[2]) +
        jointWeights[3] * skinning_fetch_matrix(chunks, jointIndices[3]);
}

#endif