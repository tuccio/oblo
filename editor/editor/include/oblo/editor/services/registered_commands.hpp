#pragma once

#include <oblo/core/deque.hpp>
#include <oblo/ecs/entity_registry.hpp>

namespace oblo
{
    struct vec3;
}

namespace oblo::editor
{
    struct command
    {
        const char* icon;
        const char* name;
    };

    struct spawn_entity_command : command
    {
        ecs::entity (*spawn)(ecs::entity_registry& reg);
    };

    struct registered_commands
    {
        deque<spawn_entity_command> spawnEntityCommands;
    };
}