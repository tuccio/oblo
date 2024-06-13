#include <oblo/editor/modules/gizmo_module.hpp>

#include <imgui.h>

#include <ImGuizmo.h>

namespace oblo::editor
{
    void gizmo_module::init()
    {
        auto& style = ImGuizmo::GetStyle();

        ImVec4 pastelRed = ImVec4(204 / 255.0f, 122 / 255.0f, 122 / 255.0f, 1.0f);
        ImVec4 pastelGreen = ImVec4(136 / 255.0f, 196 / 255.0f, 136 / 255.0f, 1.0f);
        ImVec4 pastelBlue = ImVec4(146 / 255.0f, 175 / 255.0f, 194 / 255.0f, 1.0f);
        ImVec4 pastelOrange = ImVec4(217 / 255.0f, 164 / 255.0f, 122 / 255.0f, 1.0f);

        style.Colors[ImGuizmo::COLOR::DIRECTION_X] = pastelRed;
        style.Colors[ImGuizmo::COLOR::DIRECTION_Y] = pastelGreen;
        style.Colors[ImGuizmo::COLOR::DIRECTION_Z] = pastelBlue;

        style.Colors[ImGuizmo::COLOR::PLANE_X] = pastelRed;
        style.Colors[ImGuizmo::COLOR::PLANE_Y] = pastelGreen;
        style.Colors[ImGuizmo::COLOR::PLANE_Z] = pastelBlue;

        style.Colors[ImGuizmo::COLOR::SELECTION] = pastelOrange;

        style.HatchedAxisLineThickness = 0.f;

        style.TranslationLineArrowSize = 3.f;
        style.TranslationLineThickness = 2.f;
    }

    void gizmo_module::update()
    {
        ImGuizmo::BeginFrame();
    }
}