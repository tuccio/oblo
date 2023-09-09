#include <oblo/editor/style_window.hpp>

#include <imgui.h>

namespace oblo::editor
{
    bool style_window::update()
    {
        ImGui::ShowStyleEditor();
        return true;
    }
}