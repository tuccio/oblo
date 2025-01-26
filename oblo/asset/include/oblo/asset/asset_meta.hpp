#pragma once

#include <oblo/core/string/string.hpp>
#include <oblo/core/uuid.hpp>

namespace oblo
{
    struct artifact_meta
    {
        uuid artifactId;
        uuid type;
        uuid assetId;
        string name;
    };

    struct asset_meta
    {
        uuid assetId;
        uuid mainArtifactHint;
        uuid typeHint;
        uuid nativeAssetType;
    };
}