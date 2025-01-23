#pragma once

#include <oblo/core/string/string.hpp>
#include <oblo/core/uuid.hpp>

namespace oblo
{
    struct artifact_meta
    {
        uuid id;
        uuid type;
        uuid importId;
        string importName;
    };

    struct asset_meta
    {
        uuid id;
        uuid mainArtifactHint;
        uuid typeHint;
        bool isImported;
    };
}