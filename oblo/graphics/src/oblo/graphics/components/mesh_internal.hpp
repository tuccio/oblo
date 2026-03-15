#pragma once

#include <oblo/graphics/components/static_mesh_component.hpp>
#include <oblo/math/quaternion.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/resource/resource_ptr.hpp>

namespace oblo
{
    class material;
    class mesh;
    struct skeleton;
    struct skin;

    struct mesh_resources
    {
        resource_ptr<material> material;
        resource_ptr<mesh> mesh;
        resource_ptr<skeleton> skeleton;
        resource_ptr<skin> skin;
    } OBLO_COMPONENT("77456048-9d05-4dbc-a66a-ce673d0c6c0d", Transient);

    struct mesh_processed_tag
    {
    } OBLO_TAG("5cf4add2-dd1b-458b-8d3c-e8fc0eceb489", Transient);

    struct processed_mesh_resources
    {
        static processed_mesh_resources from(const static_mesh_component& c)
        {
            return {c.material, c.mesh};
        }

        resource_ref<material> material;
        resource_ref<mesh> mesh;

        bool operator==(const processed_mesh_resources&) const = default;
    } OBLO_COMPONENT("d6546dca-5296-455b-933e-f8029d196d88", Transient);

    struct joint_pose_component
    {
        struct pose
        {
            vec3 translation;
            quaternion rotation;
            vec3 scale;
        };

        static constexpr u32 joints_per_chunk = 16;
        pose defaultPoses[joints_per_chunk];
        pose currentPoses[joints_per_chunk];
        mat4 invBindPoses[joints_per_chunk];
    } OBLO_COMPONENT("7f2b0b45-b68f-4ec7-b377-c9af94ae1d27", Transient);
}