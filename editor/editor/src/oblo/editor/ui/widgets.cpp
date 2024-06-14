#include <oblo/editor/ui/widgets.hpp>

#include <imgui_internal.h>

#include <ImGuizmo.h>

namespace oblo::editor::ui
{
    bool dragfloat_n_xyz(const char* label,
        float* v,
        int components,
        float vSpeed,
        float vMin,
        float vMax,
        const char* displayFormat,
        ImGuiSliderFlags flags)
    {
        using namespace ImGui;

        static constexpr ImU32 colors[] = {
            0xFF5757B8, // red
            0xFF6DA06D, // green
            0xFF9E8A5A, // blue
        };

        ImGuiWindow* window = GetCurrentWindow();

        if (window->SkipItems)
        {
            return false;
        }

        ImGuiContext& g = *GImGui;
        bool value_changed = false;
        BeginGroup();
        PushID(label);
        PushMultiItemsWidths(components, CalcItemWidth());
        for (int i = 0; i < components; i++)
        {

            PushID(i);
            value_changed |= DragFloat("##v", &v[i], vSpeed, vMin, vMax, displayFormat, flags);

            const ImVec2 min = GetItemRectMin();
            const ImVec2 max = GetItemRectMax();
            const float spacing = g.Style.FrameRounding;
            const float halfSpacing = spacing / 2;

            window->DrawList->AddLine({min.x + spacing, min.y - halfSpacing},
                {min.x - spacing, max.y - halfSpacing},
                colors[i],
                4);

            SameLine(0, g.Style.ItemInnerSpacing.x);
            PopID();
            PopItemWidth();
        }
        PopID();

        TextUnformatted(label, FindRenderedTextEnd(label));
        EndGroup();

        return value_changed;
    }
}