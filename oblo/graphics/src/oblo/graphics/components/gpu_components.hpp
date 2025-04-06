#pragma once

#include <oblo/core/handle.hpp>
#include <oblo/reflection/codegen/annotations.hpp>
#include <oblo/ecs/handles.hpp>
#include <oblo/math/vec3.hpp>

namespace oblo
{
    namespace vk
    {
        struct resident_texture;
    }

    struct gpu_material
    {
        vec3 albedo;
        h32<vk::resident_texture> albedoTexture;
        f32 metalness;
        f32 roughness;
        h32<vk::resident_texture> metalnessRoughnessTexture;
        h32<vk::resident_texture> normalMapTexture;
        f32 ior;
        u32 _padding[3];
        vec3 emissive;
        h32<vk::resident_texture> emissiveTexture;
    } OBLO_COMPONENT(GpuComponent = "i_MaterialBuffer");

    static_assert(sizeof(gpu_material) % 16 == 0);

    // This should not exist really, we should upload entity ids directly from the registry instead, but we currently
    // only upload component data.
    struct entity_id_component
    {
        ecs::entity entityId;
    } OBLO_COMPONENT(GpuComponent = "i_EntityIdBuffer");
}