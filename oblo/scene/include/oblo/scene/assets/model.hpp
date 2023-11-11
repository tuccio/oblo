#pragma once

#include <oblo/resource/resource_ref.hpp>
#include <oblo/scene/assets/mesh.hpp>

#include <vector>

namespace oblo
{
    struct model
    {
        std::vector<resource_ref<mesh>> meshes;
    };
}