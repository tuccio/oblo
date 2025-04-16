#pragma once

#include <oblo/resource/resource_ptr.hpp>

namespace oblo
{
    class entity_hierarchy;

    struct entity_hierarchy_loaded
    {
    } OBLO_TAG(Transient);

    struct entity_hierarchy_loading
    {
        resource_ptr<entity_hierarchy> hierarchy{};
    } OBLO_COMPONENT(Transient);
}