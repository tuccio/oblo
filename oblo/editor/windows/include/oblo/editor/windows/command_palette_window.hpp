#pragma once

#include <imgui.h>

namespace oblo::editor
{
    class editor_world;
    struct registered_commands;
    struct window_update_context;

    class viewport;

    class command_palette_window
    {
    public:
        void init(const window_update_context& ctx);
        bool update(const window_update_context& ctx);

    private:
        registered_commands* m_commands{};
        ImGuiTextFilter m_filter{};

        editor_world* m_editorWorld{};
    };
}