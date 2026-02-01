#pragma once

#include <oblo/reflection/codegen/annotations.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/resource/resource_ref.hpp>

namespace oblo
{
    class material;
    class mesh;

    struct static_mesh_component
    {
        resource_ref<mesh> mesh;
        resource_ref<material> material;
    } OBLO_COMPONENT("e9d0cda5-d199-4f4d-9572-e0ab4ab39038", ScriptAPI);
}