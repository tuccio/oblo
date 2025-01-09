#pragma once

#include <oblo/core/uuid.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/resource/resource_ref.hpp>

namespace oblo
{
    class texture;

    struct skybox_component
    {
        resource_ref<texture> texture;
        f32 multiplier;
        vec3 tint;
    };
}