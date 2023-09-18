#include <oblo/editor/windows/viewport.hpp>

#include <imgui.h>

namespace oblo::editor
{
    bool viewport::update()
    {
        bool open {true};

        if (ImGui::Begin("Viewport", &open))
        {
            ImGui::End();
        }

        return open;
    }
}