#pragma once

namespace oblo
{
    class service_registry;
}

namespace oblo::ecs
{
    /// Module service that registers runtime services, which will be instantiated into a runtime service registry.
    struct service_registrant
    {
        void (*registerServices)(service_registry& registry);
    };
}