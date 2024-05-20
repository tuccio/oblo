#pragma once

namespace oblo
{
    class asset_registry;
    class resource_registry;

    namespace ecs
    {
        class entity_registry;
    }
}

namespace oblo::smoke
{
    struct test_context_impl
    {
        ecs::entity_registry* entities{};
        asset_registry* assetRegistry{};
        resource_registry* resourceRegistry{};
        bool renderdocCapture{};
    };
}