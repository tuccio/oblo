#include <oblo/editor/ui/style.hpp>

#include <imgui.h>

namespace oblo::editor
{
    void init_ui_style()
    {
        ImGuiStyle& style = ImGui::GetStyle();

        style.Alpha = 1.0f;
        style.DisabledAlpha = 1.0f;
        style.WindowPadding = ImVec2(12.0f, 12.0f);
        style.WindowRounding = 0.0f;
        style.WindowBorderSize = 1.0f;
        style.WindowMinSize = ImVec2(20.0f, 20.0f);
        style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
        style.WindowMenuButtonPosition = ImGuiDir_None;
        style.ChildRounding = 0.0f;
        style.ChildBorderSize = 1.0f;
        style.PopupRounding = 0.0f;
        style.PopupBorderSize = 1.0f;
        style.FramePadding = ImVec2(6.0f, 6.0f);
        style.FrameRounding = 0.0f;
        style.FrameBorderSize = 0.0f;
        style.ItemSpacing = ImVec2(12.0f, 6.0f);
        style.ItemInnerSpacing = ImVec2(6.0f, 3.0f);
        style.CellPadding = ImVec2(12.0f, 6.0f);
        style.IndentSpacing = 20.0f;
        style.ColumnsMinSpacing = 6.0f;
        style.ScrollbarSize = 12.0f;
        style.ScrollbarRounding = 0.0f;
        style.GrabMinSize = 12.0f;
        style.GrabRounding = 0.0f;
        style.TabRounding = 0.0f;
        style.TabBorderSize = 0.0f;
        style.TabMinWidthForCloseButton = 0.0f;
        style.ColorButtonPosition = ImGuiDir_Right;
        style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
        style.SelectableTextAlign = ImVec2(0.0f, 0.0f);

        ImVec4* colors = style.Colors;
        colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.59f, 0.49f, 0.62f, 1.00f);
        colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.09f, 0.10f, 1.00f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.08f, 0.09f, 0.10f, 1.00f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.09f, 0.10f, 1.00f);
        colors[ImGuiCol_Border] = ImVec4(0.05f, 0.05f, 0.07f, 1.00f);
        colors[ImGuiCol_BorderShadow] = ImVec4(0.08f, 0.09f, 0.10f, 1.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.13f, 0.15f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.16f, 0.17f, 0.19f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.16f, 0.17f, 0.19f, 1.00f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.05f, 0.05f, 0.07f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.05f, 0.05f, 0.07f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.08f, 0.09f, 0.10f, 1.00f);
        colors[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.11f, 0.12f, 1.00f);
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.05f, 0.05f, 0.07f, 1.00f);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.12f, 0.13f, 0.15f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.16f, 0.17f, 0.19f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.12f, 0.13f, 0.15f, 1.00f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.90f, 0.67f, 0.95f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.20f, 0.20f, 0.21f, 1.00f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.94f, 0.79f, 0.97f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.12f, 0.13f, 0.15f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.16f, 0.17f, 0.19f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.16f, 0.17f, 0.19f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.16f, 0.17f, 0.19f, 1.00f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.16f, 0.17f, 0.19f, 1.00f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.16f, 0.17f, 0.19f, 1.00f);
        colors[ImGuiCol_Separator] = ImVec4(0.16f, 0.18f, 0.25f, 1.00f);
        colors[ImGuiCol_SeparatorHovered] = ImVec4(0.16f, 0.18f, 0.25f, 1.00f);
        colors[ImGuiCol_SeparatorActive] = ImVec4(0.16f, 0.18f, 0.25f, 1.00f);
        colors[ImGuiCol_ResizeGrip] = ImVec4(0.12f, 0.13f, 0.15f, 1.00f);
        colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.57f, 0.35f, 0.62f, 1.00f);
        colors[ImGuiCol_ResizeGripActive] = ImVec4(0.57f, 0.35f, 0.62f, 1.00f);
        colors[ImGuiCol_Tab] = ImVec4(0.05f, 0.05f, 0.07f, 1.00f);
        colors[ImGuiCol_TabHovered] = ImVec4(0.12f, 0.13f, 0.15f, 1.00f);
        colors[ImGuiCol_TabActive] = ImVec4(0.10f, 0.11f, 0.12f, 1.00f);
        colors[ImGuiCol_TabUnfocused] = ImVec4(0.05f, 0.05f, 0.07f, 1.00f);
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.08f, 0.09f, 0.10f, 1.00f);
        colors[ImGuiCol_DockingPreview] = ImVec4(0.57f, 0.35f, 0.62f, 1.00f);
        colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_PlotLines] = ImVec4(0.91f, 0.84f, 0.95f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.04f, 0.98f, 0.98f, 1.00f);
        colors[ImGuiCol_PlotHistogram] = ImVec4(1.00f, 0.29f, 0.60f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.47f, 0.70f, 1.00f);
        colors[ImGuiCol_TableHeaderBg] = ImVec4(0.05f, 0.05f, 0.07f, 1.00f);
        colors[ImGuiCol_TableBorderStrong] = ImVec4(0.05f, 0.05f, 0.07f, 1.00f);
        colors[ImGuiCol_TableBorderLight] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_TableRowBg] = ImVec4(0.12f, 0.13f, 0.15f, 1.00f);
        colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.10f, 0.11f, 0.12f, 1.00f);
        colors[ImGuiCol_TextSelectedBg] = ImVec4(0.57f, 0.35f, 0.62f, 1.00f);
        colors[ImGuiCol_DragDropTarget] = ImVec4(0.57f, 0.35f, 0.62f, 1.00f);
        colors[ImGuiCol_NavHighlight] = ImVec4(0.90f, 0.67f, 0.95f, 1.00f);
        colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.90f, 0.67f, 0.95f, 1.00f);
        colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.57f, 0.35f, 0.62f, 1.00f);
        colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.f, 0.f, 0.f, 0.f);
    }
}