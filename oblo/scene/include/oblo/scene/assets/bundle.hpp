#pragma once

#include <oblo/asset/ref.hpp>

#include <vector>

namespace oblo::scene
{
    class mesh;
    struct model;

    struct bundle
    {
        std::vector<asset::ref<mesh>> meshes;
        std::vector<asset::ref<model>> models;
    };
}