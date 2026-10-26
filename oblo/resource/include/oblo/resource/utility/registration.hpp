#pragma once

#include <oblo/core/span.hpp>

namespace oblo
{
    class resource_registry;
    class file_importers_provider;
    class resource_types_provider;

    void register_resource_types(resource_registry& registry,
        span<const resource_types_provider* const> providers);
}