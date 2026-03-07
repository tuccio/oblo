#pragma once

#include <oblo/core/handle.hpp>
#include <oblo/ecs/handles.hpp>
#include <oblo/math/mat4.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/reflection/codegen/annotations.hpp>

namespace oblo
{
    struct resident_texture;

    struct gpu_material
    {
        vec3 albedo;
        h32<resident_texture> albedoTexture;
        f32 metalness;
        f32 roughness;
        h32<resident_texture> metalnessRoughnessTexture;
        h32<resident_texture> normalMapTexture;
        f32 ior;
        u32 _padding[3];
        vec3 emissive;
        h32<resident_texture> emissiveTexture;
    } OBLO_COMPONENT("21c3e674-1189-4360-87ba-2fd95ae49cd5", GpuComponent = "i_MaterialBuffer", Transient);

    static_assert(sizeof(gpu_material) % 16 == 0);

    // This should not exist really, we should upload entity ids directly from the registry instead, but we currently
    // only upload component data.
    struct entity_id_component
    {
        ecs::entity entityId;
    } OBLO_COMPONENT("5564d553-57c2-42d7-931c-9ab1f98657d2", GpuComponent = "i_EntityIdBuffer", Transient);

    struct joint_chunk_component
    {
        static constexpr u32 max_joints_per_chunk = 32;

        mat4 jointMatrices[max_joints_per_chunk];
    } OBLO_COMPONENT("9d22f00a-1f4d-4f34-a47f-2fcc10601ceb", GpuComponent = "i_JointChunkBuffer", Transient);

    struct joint_chunk_list_component
    {
        static constexpr u32 max_joint_chunks = 32;
        ecs::entity jointChunks[max_joint_chunks];
    } OBLO_COMPONENT("7f2b0b45-b68f-4ec7-b377-c9af94ae1d27", GpuComponent = "i_JointChunkListBuffer", Transient);

    constexpr u32 get_max_joints()
    {
        return joint_chunk_list_component::max_joint_chunks * joint_chunk_component::max_joints_per_chunk;
    }
}