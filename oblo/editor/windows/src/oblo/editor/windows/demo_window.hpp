#pragma once

#include <imgui.h>

namespace oblo::editor
{
    struct window_update_context;

    class demo_window final
    {
    public:
        bool update(const window_update_context&)
        {
            bool open{true};
            ImGui::ShowDemoWindow(&open);
            return open;
        }
    };
}