#pragma once

#include <oblo/ecs/forward.hpp>

namespace oblo
{
    class service_registry;
}

namespace oblo::ecs
{
    struct world_builder
    {
        void (*services)(service_registry& registry);
        void (*systems)(system_graph_builder& builder);
    };
}