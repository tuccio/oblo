#pragma once

#include <oblo/asset/ref.hpp>
#include <oblo/scene/assets/mesh.hpp>

#include <vector>

namespace oblo::scene
{
    struct model
    {
        std::vector<asset::ref<mesh>> meshes;
    };
}