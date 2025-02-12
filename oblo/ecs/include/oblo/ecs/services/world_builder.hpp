#pragma once

#include <oblo/ecs/forward.hpp>

namespace oblo
{
    class service_registry_builder;
}

namespace oblo::ecs
{
    struct world_builder
    {
        void (*services)(service_registry_builder& builder);
        void (*systems)(system_graph_builder& builder);
    };
}