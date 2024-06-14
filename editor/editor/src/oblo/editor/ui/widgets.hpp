#pragma once

#include <imgui.h>

namespace oblo::editor::ui
{
    bool dragfloat_n_xyz(const char* label,
        float* v,
        int components,
        float vSpeed,
        float vMin = 0.f,
        float vMax = 0.f,
        const char* displayFormat = "%.3f",
        ImGuiSliderFlags flags = 0);
}
