#pragma once

namespace oblo
{
    class service_registry;
}

namespace oblo::editor
{
    struct window_update_context
    {
        service_registry& services;
    };
}