#include <oblo/editor/asset_browser.hpp>

#include <imgui.h>

namespace oblo::editor
{
    bool asset_browser::update()
    {
        bool open {true};

        if (ImGui::Begin("Asset Browser", &open))
        {
            ImGui::End();
        }

        return open;
    }
}