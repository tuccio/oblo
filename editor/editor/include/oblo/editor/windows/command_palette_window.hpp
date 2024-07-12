#pragma once

#include <imgui.h>

namespace oblo::editor
{
    struct registered_commands;
    struct window_update_context;

    class command_palette_window
    {
    public:
        void init(const window_update_context& ctx);
        bool update(const window_update_context& ctx);

    private:
        registered_commands* m_commands{};
        ImGuiTextFilter m_filter{};
    };
}