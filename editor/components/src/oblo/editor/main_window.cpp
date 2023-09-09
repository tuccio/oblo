#include <oblo/editor/main_window.hpp>

#include <imgui.h>

namespace oblo::editor
{
    bool main_window::update()
    {
        // ImGuiWindowFlags windowFlags{};

        // const ImGuiViewport* viewport = ImGui::GetMainViewport();

        // ImGui::SetNextWindowPos(viewport->WorkPos);
        // ImGui::SetNextWindowSize(viewport->WorkSize);
        // ImGui::SetNextWindowViewport(viewport->ID);
        // ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        // ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

        // windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
        //                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        // bool open{true};
        // ImGui::Begin("Main Window", &open, windowFlags);

        // ImGui::End();

        ImGui::ShowDemoWindow();
        return true;
    }
}