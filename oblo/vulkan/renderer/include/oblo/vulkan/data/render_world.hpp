#pragma once

namespace oblo::ecs
{
    class entity_registry;
}

namespace oblo::vk
{
    class draw_registry;

    struct render_world
    {
        ecs::entity_registry* entityRegistry;
        draw_registry* drawRegistry;
    };
}