#pragma once

#include <span>

namespace oblo
{
    class resource_registry;
    class file_importers_provider;
    class resource_types_provider;

    void register_resource_types(resource_registry& registry,
        std::span<const resource_types_provider* const> providers);
}