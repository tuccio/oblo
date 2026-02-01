#pragma once

#include <oblo/ecs/handles.hpp>
#include <oblo/reflection/codegen/annotations.hpp>

namespace oblo
{
    struct parent_component
    {
        ecs::entity parent;
    } OBLO_COMPONENT("a8ae7a83-d1ed-44ab-a3c5-b585857dcf1f", ScriptAPI);
}