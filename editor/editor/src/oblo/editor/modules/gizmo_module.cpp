#include <oblo/editor/modules/gizmo_module.hpp>

#include <imgui.h>

#include <ImGuizmo.h>

namespace oblo::editor
{
    void gizmo_module::init()
    {
        auto& style = ImGuizmo::GetStyle();

        ImVec4 pastelRed = ImVec4(184 / 255.0f, 87 / 255.0f, 87 / 255.0f, 1.0f);
        ImVec4 pastelGreen = ImVec4(109 / 255.0f, 160 / 255.0f, 109 / 255.0f, 1.0f);
        ImVec4 pastelBlue = ImVec4(90 / 255.0f, 138 / 255.0f, 158 / 255.0f, 1.0f);
        ImVec4 pastelPurple = ImVec4(201 / 255.0f, 127 / 255.0f, 189 / 255.0f, 1.0f);

        style.Colors[ImGuizmo::COLOR::DIRECTION_X] = pastelRed;
        style.Colors[ImGuizmo::COLOR::DIRECTION_Y] = pastelGreen;
        style.Colors[ImGuizmo::COLOR::DIRECTION_Z] = pastelBlue;

        style.Colors[ImGuizmo::COLOR::PLANE_X] = pastelRed;
        style.Colors[ImGuizmo::COLOR::PLANE_Y] = pastelGreen;
        style.Colors[ImGuizmo::COLOR::PLANE_Z] = pastelBlue;

        style.Colors[ImGuizmo::COLOR::SELECTION] = pastelPurple;

        style.HatchedAxisLineThickness = 0.f;

        style.TranslationLineArrowSize = 3.f;
        style.TranslationLineThickness = 2.f;
    }

    void gizmo_module::update()
    {
        ImGuizmo::BeginFrame();
    }
}