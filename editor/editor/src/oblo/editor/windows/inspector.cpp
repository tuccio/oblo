#include <oblo/editor/windows/inspector.hpp>

#include <imgui.h>

namespace oblo::editor
{
    bool inspector::update(const window_update_context&)
    {
        bool open{true};

        if (ImGui::Begin("Inspector", &open))
        {
            ImGui::End();
        }

        return open;
    }
}