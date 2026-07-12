#pragma once

#include <oblo/editor/services/asset_editor.hpp>
#include <oblo/editor/services/resource_viewer.hpp>

namespace oblo::editor
{
    class material_editor final : public asset_editor
    {
    public:
        expected<> open(window_manager& wm, asset_registry& assetRegistry, window_handle parent, uuid assetId) override;

        void close(window_manager& wm) override;

        expected<> save(window_manager& wm, asset_registry& assetRegistry) override;

        window_handle get_window() const override;

    private:
        window_handle m_editor{};
    };

    class material_viewer final : public resource_viewer
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