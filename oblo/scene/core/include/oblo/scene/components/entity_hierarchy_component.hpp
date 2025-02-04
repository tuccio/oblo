#pragma once

#include <oblo/resource/resource_ref.hpp>

namespace oblo
{
    class entity_hierarchy;

    struct entity_hierarchy_component
    {
        resource_ref<entity_hierarchy> hierarchy;
    };
}