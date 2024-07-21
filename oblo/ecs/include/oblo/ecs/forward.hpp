#pragma once

#include <oblo/ecs/handles.hpp>

namespace oblo::ecs
{
    class entity_registry;
    class system_graph_builder;
    class type_registry;

    struct service_registrant;
    struct system_update_context;

    using world_builder = void (*)(system_graph_builder& b);
}