#pragma once

#include <oblo/editor/services/asset_editor.hpp>

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
}