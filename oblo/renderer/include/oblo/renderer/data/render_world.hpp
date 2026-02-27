#pragma once

namespace oblo::ecs
{
    class entity_registry;
}

namespace oblo
{
    class draw_registry;

    struct render_world
    {
        ecs::entity_registry* entityRegistry;
        draw_registry* drawRegistry;
    };
}