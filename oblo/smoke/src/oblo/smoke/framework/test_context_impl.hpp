#pragma once

#include <oblo/ecs/forward.hpp>

namespace oblo
{
    class asset_registry;
    class resource_registry;
}

namespace oblo::smoke
{
    struct test_context_impl
    {
        ecs::entity_registry* entities{};
        asset_registry* assetRegistry{};
        resource_registry* resourceRegistry{};
        ecs::entity cameraEntity;
        bool renderdocCapture{};
    };
}