#pragma once

#include <oblo/reflection/codegen/annotations.hpp>
#include <oblo/resource/resource_ref.hpp>

namespace oblo
{
    class entity_hierarchy;

    struct entity_hierarchy_component
    {
        resource_ref<entity_hierarchy> hierarchy;
    } OBLO_COMPONENT();
}