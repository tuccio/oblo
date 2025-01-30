#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/resource/resource_ref.hpp>
#include <oblo/scene/resources/traits.hpp>

namespace oblo
{
    class material;
    class mesh;

    struct model
    {
        dynamic_array<resource_ref<mesh>> meshes;
        dynamic_array<resource_ref<material>> materials;
    };
}