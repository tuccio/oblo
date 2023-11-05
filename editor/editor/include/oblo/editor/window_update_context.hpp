#pragma once

#include <oblo/editor/window_handle.hpp>

namespace oblo::editor
{
    class service_context;
    class window_manager;

    struct window_update_context
    {
        window_manager& windowManager;
        window_handle windowHandle;
        const service_context& services;
    };
}