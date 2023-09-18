#include <oblo/editor/windows/scene_hierarchy.hpp>

#include <imgui.h>

namespace oblo::editor
{
    bool scene_hierarchy::update()
    {
        bool open {true};

        if (ImGui::Begin("Hierarchy", &open))
        {
            ImGui::End();
        }

        return open;
    }
}