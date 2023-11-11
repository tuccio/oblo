#pragma once

#include <oblo/asset/asset_ref.hpp>
#include <oblo/scene/assets/mesh.hpp>

#include <vector>

namespace oblo
{
    struct model
    {
        std::vector<asset_ref<mesh>> meshes;
    };
}