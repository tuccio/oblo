#pragma once

#include <oblo/core/buffered_array.hpp>
#include <oblo/ecs/handles.hpp>
#include <oblo/reflection/codegen/annotations.hpp>

namespace oblo
{
    struct children_component
    {
        buffered_array<ecs::entity, 8> children;
    } OBLO_COMPONENT(ScriptAPI);
}