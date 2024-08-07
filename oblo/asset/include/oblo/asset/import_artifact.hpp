#pragma once

#include <oblo/asset/any_asset.hpp>
#include <oblo/core/string/string.hpp>
#include <oblo/core/uuid.hpp>

namespace oblo
{
    struct import_artifact
    {
        uuid id;
        any_asset data;
        string name;
    };
}