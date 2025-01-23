#pragma once

#include <oblo/core/string/string.hpp>
#include <oblo/core/uuid.hpp>

namespace oblo
{
    struct artifact_meta
    {
        uuid artifactId;
        uuid type;
        uuid sourceFileId;
        uuid assetId;
        string importName;
    };

    struct asset_meta
    {
        uuid assetId;
        uuid sourceFileId;
        uuid mainArtifactHint;
        uuid typeHint;
        bool isImported;
    };
}