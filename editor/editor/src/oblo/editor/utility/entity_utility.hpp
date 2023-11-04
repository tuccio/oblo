#pragma once

#include <oblo/ecs/handles.hpp>

namespace oblo::ecs
{
    class entity_registry;
}

namespace oblo::editor::entity_utility
{
    const char* get_name_cstr(ecs::entity_registry& reg, ecs::entity e);
}