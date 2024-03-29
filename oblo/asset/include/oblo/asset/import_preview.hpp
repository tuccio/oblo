#pragma once

#include <oblo/core/type_id.hpp>
#include <oblo/core/types.hpp>
#include <oblo/core/uuid.hpp>

#include <span>
#include <string>
#include <vector>

namespace oblo
{
    class asset_registry;

    struct import_node
    {
        type_id type;
        std::string name;
    };

    struct import_node_config
    {
        bool enabled;
        uuid id;
    };

    struct import_preview
    {
        std::vector<import_node> nodes;
    };

    struct import_context
    {
        asset_registry* registry;
        std::span<const import_node> nodes;
        std::span<const import_node_config> importNodesConfig;
        uuid importUuid;
    };
}