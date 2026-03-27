#pragma once

#include <oblo/editor/services/resource_viewer.hpp>

namespace oblo::editor
{
    class mesh_inspector final : public resource_viewer
    {
    public:
        expected<> open(window_manager& wm,
            const resource_registry& resourceRegistry,
            window_handle parent,
            uuid resourceId) override;

        void close(window_manager& wm) override;

        window_handle get_window() const override;

    private:
        window_handle m_editor{};
    };
}