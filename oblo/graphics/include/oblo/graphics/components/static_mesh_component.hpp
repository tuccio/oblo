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
    } OBLO_COMPONENT(ScriptAPI);
}