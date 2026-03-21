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

    struct joint_skinning_transform_chunks_component
    {
        static constexpr u32 max_chunks = 16;
        ecs::entity chunks[max_chunks];
        u32 numJoints;
    } OBLO_COMPONENT("60f4d010-5c1c-4629-958d-14b61156b1bc", GpuComponent = "i_JointSkinningChunksBuffer", Transient);

    struct joint_skinning_transform_component
    {
        static constexpr u32 joints_per_chunk = 16;
        mat4 jointMatrices[joints_per_chunk];
    } OBLO_COMPONENT(
        "d7f5e8b3-8a31-4d7c-b3af-8fcdb0b56ccf", GpuComponent = "i_JointSkinningTransformBuffer", Transient);
}