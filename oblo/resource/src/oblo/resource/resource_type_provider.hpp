#pragma once

#include <oblo/core/dynamic_array.hpp>

namespace oblo
{
    struct resource_type_descriptor;

    class resource_type_provider
    {
    public:
        virtual ~resource_type_provider() = default;

        virtual void fetch_resource_types(dynamic_array<resource_type_descriptor>& outResourceTypes) = 0;
    };
}