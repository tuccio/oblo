#pragma once

#include <oblo/ecs/handles.hpp>
#include <oblo/reflection/codegen/annotations.hpp>

namespace oblo
{
    struct parent_component
    {
        ecs::entity parent;
    } OBLO_COMPONENT();
}