#include <oblo/editor/ui/widgets.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/editor/ui/constants.hpp>

#include <imgui_internal.h>

namespace oblo::editor::ui
{
    namespace
    {
        static constexpr ImU32 g_xyzColors[] = {
            colors::red,
            colors::green,
            colors::blue,
        };
    }

    bool dragfloat_n_xyz(const char* label,
        float* v,
        int components,
        float vSpeed,
        float vMin,
        float vMax,
        const char* displayFormat,
        ImGuiSliderFlags flags)
    {
        OBLO_ASSERT(components >= 0 && components <= 3);

        using namespace ImGui;

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
            const f32 spacing = g.Style.FrameRounding;
            const f32 halfSpacing = spacing / 2;
            constexpr f32 thickness = 4.f;

            window->DrawList->AddLine({min.x + spacing, min.y - halfSpacing},
                {min.x - spacing, max.y - halfSpacing},
                g_xyzColors[i],
                thickness);

            SameLine(0, g.Style.ItemInnerSpacing.x);
            PopID();
            PopItemWidth();
        }

        PopID();

        TextUnformatted(label, FindRenderedTextEnd(label));
        EndGroup();

        return value_changed;
    }

    bool toggle_button(const char* label, bool* enabled, const ImVec2& size)
    {
        const bool isEnabled = *enabled;

        bool wasPressed;

        if (isEnabled)
        {
            const auto color = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::PushStyleColor(ImGuiCol_Border, color);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
        }

        wasPressed = ImGui::Button(label, size);

        if (wasPressed)
        {
            *enabled = !*enabled;
        }

        if (isEnabled)
        {
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar();
        }

        return wasPressed;
    }
}