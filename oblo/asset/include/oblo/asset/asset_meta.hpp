#pragma once

#include <oblo/core/type_id.hpp>
#include <oblo/core/uuid.hpp>

#include <filesystem>
#include <vector>

namespace oblo
{
    struct artifact_meta
    {
        uuid id;
        type_id type;
        uuid importId;
        std::string importName;
    };

    struct asset_meta
    {
        uuid id;
        type_id type;
        uuid importId;
        std::string importName;
    };
}