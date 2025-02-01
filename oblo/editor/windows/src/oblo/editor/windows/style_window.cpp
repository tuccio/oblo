#include <oblo/editor/windows/style_window.hpp>

#include <imgui.h>

namespace oblo::editor
{
    bool style_window::update(const window_update_context&)
    {
        ImGui::ShowStyleEditor();
        return true;
    }
}