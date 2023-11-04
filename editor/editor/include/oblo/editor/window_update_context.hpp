#pragma once

namespace oblo::editor
{
    class service_context;

    struct window_update_context
    {
        const service_context& services;
    };
}