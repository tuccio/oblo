#pragma once

#include <oblo/modules/utility/provider_service.hpp>

namespace oblo
{
    class service_registry;
}

namespace oblo::editor
{
    using register_services_fn = void (*)(service_registry& registry);

    struct service_provider_descriptor
    {
        register_services_fn registerServices{};
    };

    using service_provider = provider_service<service_provider_descriptor>;
}