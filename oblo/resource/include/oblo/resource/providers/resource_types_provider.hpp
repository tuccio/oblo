#pragma once

#include <oblo/core/deque.hpp>

namespace oblo
{
    struct resource_type_descriptor;

    class resource_types_provider
    {
    public:
        virtual ~resource_types_provider() = default;

        virtual void fetch_resource_types(deque<resource_type_descriptor>& outResourceTypes) const = 0;
    };
}