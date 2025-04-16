#pragma once

#include <oblo/graphics/components/static_mesh_component.hpp>
#include <oblo/resource/resource_ptr.hpp>

namespace oblo
{
    struct mesh_resources
    {
        resource_ptr<material> material;
        resource_ptr<mesh> mesh;
    } OBLO_COMPONENT(Transient);

    struct mesh_processed_tag
    {
    } OBLO_TAG(Transient);

    struct processed_mesh_resources
    {
        static processed_mesh_resources from(const static_mesh_component& c)
        {
            return {c.material, c.mesh};
        }

        resource_ref<material> material;
        resource_ref<mesh> mesh;

        bool operator==(const processed_mesh_resources&) const = default;
    } OBLO_COMPONENT(Transient);
}