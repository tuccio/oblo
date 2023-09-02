#pragma once

#include <oblo/core/type_id.hpp>
#include <oblo/core/uuid.hpp>

#include <filesystem>
#include <vector>

namespace oblo::asset
{
    struct artifact_meta
    {
        uuid id;
        type_id type;
        std::string name;
    };

    struct asset_meta
    {
        type_id type;
        std::vector<artifact_meta> artifacts;
    };
}