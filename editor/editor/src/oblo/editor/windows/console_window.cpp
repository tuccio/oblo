#include <oblo/editor/windows/console_window.hpp>

#include <oblo/editor/service_context.hpp>
#include <oblo/editor/services/log_queue.hpp>
#include <oblo/editor/window_update_context.hpp>

#include <imgui.h>
#include <imgui_internal.h>

// TODO: Remove
#include <oblo/log/log.hpp>

namespace oblo::editor
{
    namespace
    {
        constexpr cstring_view g_severityStrings[]{
            "Debug",
            "Info",
            "Warn",
            "Error",
        };

        static constexpr ImU32 g_severityColors[] = {
            0xFF9E8A5A, // blue
            0xFF6DA06D, // green
            0xFF7CC9E6, // orange
            // 0xFFEEDE66, // orange
            0xFF5757B8, // red
        };

        void draw_message(log::severity severity, cstring_view msg)
        {
            using namespace ImGui;

            ImGuiWindow* window = GetCurrentWindow();

            if (window->SkipItems)
            {
                return;
            }

            ImGuiContext& g = *GImGui;
            BeginGroup();
            // PushID(label);
            // PushMultiItemsWidths(components, CalcItemWidth());

            auto table = GetCurrentTable();

            {
                /*              const ImVec2 min1 = GetItemRectMin();
                              const ImVec2 max1 = GetItemRectMax();*/

                // PushID(i);
                // value_changed |= DragFloat("##v", &v[i], vSpeed, vMin, vMax, displayFormat, flags);
                // SameLine(0, g.Style.ItemInnerSpacing.x);
                TextUnformatted(msg.data(), FindRenderedTextEnd(msg.begin(), msg.end()));

                const ImVec2 min = GetItemRectMin();
                const ImVec2 max = GetItemRectMax();

                const auto color = g_severityColors[u32(severity)];

                constexpr f32 thickness = 4.f;
                // constexpr f32 spacing = 2 * thickness;

                const auto& clipRect = TableGetCellBgRect(table, 0);
                // const auto& clipRect = table->Columns[0].ClipRect;

                /*      window->DrawList->AddLine({clipRect.Min.x, clipRect.Min.y},
                          {clipRect.Min.x, clipRect.Max.y},
                          color,
                          thickness);*/

                const f32 x = clipRect.Min.x + .5f * thickness;

                window->DrawList->AddLine({x, min.y - g.Style.CellPadding.y},
                    {x, max.y + g.Style.CellPadding.y},
                    color,
                    thickness);

                // window->DrawList->AddLine({clipRect.Min.x, min.y}, {clipRect.Min.x, max.y}, color, thickness);

                // SameLine(0, g.Style.ItemInnerSpacing.x);
                //  PopID();
                //  PopItemWidth();
            }

            // PopID();

            EndGroup();
        }
    }

    void console_window::init(const window_update_context& ctx)
    {
        m_logQueue = ctx.services.find<const log_queue>();
        OBLO_ASSERT(m_logQueue);
    }

    bool console_window::update(const window_update_context&)
    {
        bool open{true};

        // TODO: Remove
        {
            static u32 i = 0;
            constexpr u32 N = 60;

            if (i++ % N == 0)
            {
                log::severity s{(i / N) % (u32(log::severity::error) + 1)};
                log::generic(s, "Test");
            }
        }

        if (ImGui::Begin("Console", &open))
        {
            if (ImGui::BeginTable("#logs", 1, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders))
            {
                const bool autoScroll = ImGui::GetScrollY() >= ImGui::GetScrollMaxY();

                // ImGui::TableSetupColumn("Severity", ImGuiTableColumnFlags_WidthFixed, .0001f);
                ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_None);

                const auto& messages = m_logQueue->get_messages();

                for (usize i = 0; i < messages.size(); ++i)
                {
                    const auto& message = messages[i];

                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);

                    const auto severity = g_severityStrings[u32(message.severity)];
                    (void) severity;

                    // const auto color = g_severityColors[u32(message.severity)];
                    // ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, color);

                    // ImGui::TableSetColumnIndex(1);

                    draw_message(message.severity, message.content);
                    // ImGui::TextUnformatted(message.content.begin(), message.content.end());
                }

                if (autoScroll)
                {
                    ImGui::SetScrollHereY();
                }
            }

            ImGui::EndTable();
        }

        ImGui::End();

        return open;
    }
}