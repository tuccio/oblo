#pragma once

namespace oblo::editor
{
    class asset_editor_manager;
    struct window_update_context;

    class editor_window final
    {
    public:
        void init(const window_update_context& ctx);
        bool update(const window_update_context& ctx);

    private:
        asset_editor_manager* m_assetEditorManager{};
    };
}