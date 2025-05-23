#pragma once

#include <oblo/asset/import/import_config.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/string/string.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/types.hpp>
#include <oblo/core/uuid.hpp>

namespace oblo
{
    struct import_node
    {
        uuid artifactType;
        string name;
    };

    struct import_node_config
    {
        bool enabled;
        uuid id;
    };

    struct import_preview
    {
        dynamic_array<import_node> nodes;
        dynamic_array<import_config> children;
    };
}