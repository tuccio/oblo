#pragma once

#include <oblo/resource/resource_ptr.hpp>

namespace oblo
{
    class entity_hierarchy;

    struct entity_hierarchy_loaded
    {
    } OBLO_TAG("3fd7b448-9ccd-4982-ba72-86069f8375d9", Transient);

    struct entity_hierarchy_loading
    {
        resource_ptr<entity_hierarchy> hierarchy{};
    } OBLO_COMPONENT("71904e4f-cea4-4a1f-865f-d348c052c428", Transient);
}