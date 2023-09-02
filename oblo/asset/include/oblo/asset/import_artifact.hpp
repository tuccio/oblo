#pragma once

#include <oblo/asset/any_asset.hpp>
#include <oblo/core/uuid.hpp>

#include <string>

namespace oblo::asset
{
    struct import_artifact
    {
        uuid id;
        any_asset data;
        std::string name;
    };
}