#ifndef OBLO_INCLUDE_RENDERER_MESHES_SKINNING
#define OBLO_INCLUDE_RENDERER_MESHES_SKINNING

#include <ecs/entity>
#include <ecs/entity_set>
#include <renderer/debug/printf>
#include <renderer/instance_id>
#include <renderer/instances>
#include <renderer/meshes/mesh_table>

// This needs to match joint_skinning_transform_chunks_component::max_chunks
const uint g_skinningMaxChunks = 16;

// This needs to match joint_skinning_transform_component::joints_per_chunk
const uint g_skinningJointsPerChunk = 16;

struct skinning_joint_chunks
{
    ecs_entity chunks[g_skinningMaxChunks];
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