#pragma once

#include <oblo/math/vec2u.hpp>

namespace oblo::editor
{
    class asset_editor_manager;
    struct window_update_context;

    enum class editor_window_event
    {
        none,
        minimize,
        maximize,
        restore,
        close,
    };

    class editor_window final
    {
    public:
        void init(const window_update_context& ctx);
        bool update(const window_update_context& ctx);

        editor_window_event get_last_window_event() const;
        bool is_draggable_space(const vec2u& position) const;

        void set_is_maximized(bool isMaximized);

    private:
        asset_editor_manager* m_assetEditorManager{};

        editor_window_event m_lastEvent{};
        vec2u m_draggableAreaMin{};
        vec2u m_draggableAreaMax{};

        u64 m_appIconId{};

        bool m_isMaximized{};
    };
}