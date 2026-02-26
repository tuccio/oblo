#pragma once

#include <oblo/core/handle.hpp>
#include <oblo/ecs/handles.hpp>
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
}