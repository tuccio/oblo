#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/editor/window_handle.hpp>

namespace oblo
{
    class resource_registry;
}

namespace oblo::editor
{
    class window_manager;

    class resource_viewer
    {
    public:
        virtual ~resource_viewer() = default;

        virtual expected<> open(
            window_manager& wm, const resource_registry& resourceRegistry, window_handle parent, uuid resourceId) = 0;

        virtual void close(window_manager& wm) = 0;

        virtual window_handle get_window() const = 0;
    };
}