#pragma once

#include <oblo/resource/resource_ref.hpp>

#include <vector>

namespace oblo
{
    class material;
    class mesh;

    struct model
    {
        std::vector<resource_ref<mesh>> meshes;
        std::vector<resource_ref<material>> materials;
    };
}