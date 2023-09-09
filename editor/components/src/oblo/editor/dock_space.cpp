#include <oblo/editor/dock_space.hpp>

#include <imgui.h>

namespace oblo::editor
{
    bool dock_space::update()
    {
        constexpr ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_None;

        // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
        // because it would be confusing to have two docking targets within each others.
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

        windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                       ImGuiWindowFlags_NoMove;

        windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background
        // and handle the pass-thru hole, so we ask Begin() to not render a background.
        if (dockspaceFlags & ImGuiDockNodeFlags_PassthruCentralNode)
            windowFlags |= ImGuiWindowFlags_NoBackground;

        // Important: note that we proceed even if Begin() returns false (aka window is collapsed).
        // This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
        // all active windows docked into it will lose their parent and become undocked.
        // We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
        // any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
        // if (!opt_padding)
        // {
        // }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGui::Begin("DockSpace", nullptr, windowFlags);

        // if (!opt_padding)
        // {
        //     ImGui::PopStyleVar();
        // }

        // if (opt_fullscreen)
        // {
        // Pop ImGuiStyleVar_WindowPadding, ImGuiStyleVar_WindowRounding and ImGuiStyleVar_WindowBorderSize
        ImGui::PopStyleVar(3);
        // }

        // Submit the DockSpace
        ImGuiID dockspace_id = ImGui::GetID("oblo_dockspace");
        ImGui::DockSpace(dockspace_id, ImVec2{0.f, 0.f}, dockspaceFlags);

        ImGui::End();

        return true;
    }
}