#pragma once

#include <span>

namespace oblo
{
    class asset_registry;
    class file_importers_provider;
    class resource_types_provider;

    void register_asset_types(asset_registry& registry, std::span<const resource_types_provider* const> providers);
    void register_file_importers(asset_registry& registry, std::span<const file_importers_provider* const> providers);
}