#pragma once

#include <oblo/core/string/string.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/uuid.hpp>

namespace oblo
{
    struct artifact_meta
    {
        uuid id;
        type_id type;
        uuid importId;
        string importName;
    };

    struct asset_meta
    {
        uuid id;
        uuid mainArtifactHint;
        type_id typeHint;
        bool isImported;
    };
}