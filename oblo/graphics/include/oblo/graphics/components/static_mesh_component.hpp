#pragma once

#include <oblo/core/uuid.hpp>

namespace oblo::graphics
{
    struct static_mesh_component
    {
        uuid mesh;
        u32 instance; // Just temporarily here
    };
}