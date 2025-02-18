#pragma once

#include <imgui.h>

namespace oblo::editor::ui
{
    bool dragfloat_n_xyzw(const char* label,
        float* v,
        int components,
        float vSpeed,
        float vMin = 0.f,
        float vMax = 0.f,
        const char* displayFormat = "%.3f",
        ImGuiSliderFlags flags = 0);

    bool toggle_button(const char* label, bool* enabled, const ImVec2& size = ImVec2(0, 0));
}
