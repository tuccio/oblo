#pragma once

#include <oblo/core/uuid.hpp>
#include <oblo/resource/resource_ref.hpp>

namespace oblo
{
    class texture;

    struct skybox_component
    {
        resource_ref<texture> texture;
    };
}