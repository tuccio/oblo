#pragma once

#include <oblo/core/uuid.hpp>
#include <oblo/resource/resource_ref.hpp>

namespace oblo
{
    class mesh;

    struct static_mesh_component
    {
        resource_ref<mesh> mesh;
        u32 instance; // Just temporarily here
    };
}