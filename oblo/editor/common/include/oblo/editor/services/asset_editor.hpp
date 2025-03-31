#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/editor/window_handle.hpp>

namespace oblo::editor
{
    class window_manager;

    class asset_editor
    {
    public:
        virtual ~asset_editor() = default;

        virtual expected<> open(window_manager& wm, uuid assetId) = 0;

        virtual void close(window_manager& wm) = 0;

        virtual expected<> save(window_manager& wm) = 0;

        virtual window_handle get_window() const = 0;
    };
}