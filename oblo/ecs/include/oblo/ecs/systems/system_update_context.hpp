#pragma once

namespace oblo
{
    class frame_allocator;
    class service_registry;
}

namespace oblo::ecs
{
    class entity_registry;

    struct system_update_context
    {
        entity_registry* entities;
        service_registry* services;
        frame_allocator* frameAllocator;
    };
}