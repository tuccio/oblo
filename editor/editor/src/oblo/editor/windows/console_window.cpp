#include <oblo/editor/windows/console_window.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/core/flags.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/editor/service_context.hpp>
#include <oblo/editor/services/log_queue.hpp>
#include <oblo/editor/ui/constants.hpp>
#include <oblo/editor/ui/widgets.hpp>
#include <oblo/editor/window_update_context.hpp>

#include <imgui.h>
#include <imgui_internal.h>

#include <IconsFontAwesome6.h>

#include <algorithm>

namespace oblo::editor
{
    namespace
    {
        constexpr u32 g_severityCount = 4;

        constexpr cstring_view g_severityStrings[g_severityCount]{
            "Debug",
            "Info",
            "Warn",
            "Error",
        };

        static constexpr ImU32 g_severityColors[g_severityCount] = {
            colors::blue,
            colors::green,
            colors::yellow,
            colors::red,
        };

        void draw_severity_accent(log::severity severity)
        {
            ImGuiWindow* window = ImGui::GetCurrentWindow();

            if (window->SkipItems)
            {
                return;
            }

            const ImVec2 min = ImGui::GetItemRectMin();
            const ImVec2 max = ImGui::GetItemRectMax();

            const auto color = g_severityColors[u32(severity)];

            constexpr f32 thickness = 4.f;

            auto table = ImGui::GetCurrentTable();
            const auto& clipRect = ImGui::TableGetCellBgRect(table, 0);

            const f32 x = clipRect.Min.x;

            ImGuiContext& g = *GImGui;

            window->DrawList->AddLine({x, min.y - g.Style.CellPadding.y},
                {x, max.y + g.Style.CellPadding.y},
                color,
                thickness);
        }

        void draw_message(log::severity severity, cstring_view msg)
        {
            ImGui::TextUnformatted(msg.begin(), msg.end());
            draw_severity_accent(severity);
        }
    }

    class console_window::filter
    {
    public:
        void update(const deque<log_queue::message>& messages)
        {
            bool needsRebuild = false;

            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && ImGui::IsKeyPressed(ImGuiKey_F) &&
                ImGui::IsKeyDown(ImGuiMod_Ctrl))
            {
                ImGui::SetKeyboardFocusHere();
            }

            if (ImGui::InputTextWithHint("##search",
                    "Filter ... " ICON_FA_MAGNIFYING_GLASS,
                    m_impl.InputBuf,
                    array_size(m_impl.InputBuf)))
            {
                m_impl.Build();
                needsRebuild = true;
            }

            ImGui::SameLine();

            if (ImGui::Button(ICON_FA_XMARK))
            {
                m_impl.Clear();
                needsRebuild = true;
            }

            ImGui::SetItemTooltip("Clear filter");

            ImGui::SameLine();

            ImGui::Button(ICON_FA_FILTER);

            ImGui::SetItemTooltip("Filter");

            if (ImGui::BeginPopupContextItem(nullptr, ImGuiPopupFlags_MouseButtonLeft))
            {
                bool selectAll = m_disabledSeverities.is_empty();

                if (ImGui::Checkbox("Select all", &selectAll))
                {
                    if (selectAll)
                    {
                        m_disabledSeverities = {};
                    }
                    else
                    {
                        m_disabledSeverities = {};
                        m_disabledSeverities = ~m_disabledSeverities;
                    }

                    needsRebuild = true;
                }

                for (u32 i = 0; i < g_severityCount; ++i)
                {
                    const auto severity = log::severity(i);

                    bool isEnabled = !m_disabledSeverities.contains(severity);
                    if (ImGui::Checkbox(g_severityStrings[i].c_str(), &isEnabled))
                    {
                        m_disabledSeverities.assign(severity, !isEnabled);
                        needsRebuild = true;
                    }
                }

                ImGui::EndPopup();
            }

            if (needsRebuild)
            {
                rebuild(messages);
            }
            else
            {
                incremental_filter(messages);
            }
        }

        bool is_active() const
        {
            return m_isActive;
        }

        usize get_filtered_count() const
        {
            return m_filteredIndices.size();
        }

        usize get_filtered_index(usize i) const
        {
            return m_filteredIndices[i];
        }

    private:
        void rebuild(const deque<log_queue::message>& messages)
        {
            m_lastProcessedMessage = 0;
            m_filteredIndices.clear();
            m_isActive = m_impl.IsActive() || !m_disabledSeverities.is_empty();
            incremental_filter(messages);
        }

        void incremental_filter(const deque<log_queue::message>& messages)
        {
            if (!is_active())
            {
                return;
            }

            for (; m_lastProcessedMessage < messages.size(); ++m_lastProcessedMessage)
            {
                const auto& msg = messages[m_lastProcessedMessage];

                if (!m_disabledSeverities.contains(msg.severity) &&
                    m_impl.PassFilter(msg.content.begin(), msg.content.end()))
                {
                    m_filteredIndices.emplace_back(m_lastProcessedMessage);
                }
            }
        }

    private:
        ImGuiTextFilter m_impl{};
        usize m_lastProcessedMessage{};
        deque<usize> m_filteredIndices;
        flags<log::severity, g_severityCount> m_disabledSeverities{};
        bool m_isActive{};
    };

    console_window::console_window() = default;

    console_window::~console_window() = default;

    void console_window::init(const window_update_context& ctx)
    {
        m_logQueue = ctx.services.find<const log_queue>();
        OBLO_ASSERT(m_logQueue);

        m_filter = std::make_unique<filter>();
    }

    bool console_window::update(const window_update_context&)
    {
        bool open{true};

        if (ImGui::Begin("Console", &open, ImGuiWindowFlags_NoScrollbar))
        {
            const auto& messages = m_logQueue->get_messages();

            m_filter->update(messages);

            ImGui::SameLine();

            const bool autoScrollSetByUser = ui::toggle_button(ICON_FA_ARROWS_DOWN_TO_LINE, &m_autoScroll);
            ImGui::SetItemTooltip("Toggle auto-scroll");

            if (ImGui::BeginTable("#logs",
                    1,
                    ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY))
            {
                if (!autoScrollSetByUser && ImGui::GetScrollY() < ImGui::GetScrollMaxY())
                {
                    m_autoScroll = false;
                }

                ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_None);

                string_builder buf;

                const auto maxMessages =
                    narrow_cast<int>(m_filter->is_active() ? m_filter->get_filtered_count() : messages.size());

                ImGuiListClipper clipper;
                clipper.Begin(maxMessages);

                if (m_autoScroll && !messages.empty())
                {
                    clipper.IncludeItemByIndex(clipper.ItemsCount - 1);
                }

                while (clipper.Step())
                {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
                    {
                        int messageIndex = i;

                        if (m_filter->is_active())
                        {
                            messageIndex = narrow_cast<int>(m_filter->get_filtered_index(usize(i)));
                            OBLO_ASSERT(messageIndex < messages.size() && messageIndex >= 0);
                        }

                        const auto& message = messages[usize(messageIndex)];

                        ImGui::TableNextRow();

                        ImGui::TableSetColumnIndex(0);

                        cstring_view msg = message.content;

                        if (auto it = std::find(message.content.begin(), message.content.end(), '\n');
                            it != message.content.end())
                        {
                            buf.clear();

                            auto prev = message.content.begin();

                            do
                            {
                                buf.append(prev, it);
                                buf.append(" " ICON_FA_ANGLE_DOWN " ");

                                for (++it; it != message.content.end() && (*it == '\n' || *it == '\r'); ++it)
                                {
                                }

                                prev = it;
                                it = std::find(prev, message.content.end(), '\n');
                            } while (it != message.content.end());

                            buf.append(prev, it);
                            msg = buf;
                        }

                        draw_message(message.severity, msg);

                        const auto selectableHeight = ImGui::GetItemRectSize().y;

                        ImGui::SameLine();

                        if (ImGui::Selectable(buf.clear().format("##item{}", i).c_str(),
                                m_selected == i,
                                ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick,
                                {0, selectableHeight}))
                        {
                            m_selected = i;

                            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                            {
                                ImGui::OpenPopup(buf.clear().format("##logctx{}", i).c_str());
                            }
                        }

                        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{});

                        if (ImGui::BeginPopup(buf.clear().format("##logctx{}", i).c_str(),
                                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking))
                        {
                            if (ImGui::IsWindowAppearing())
                            {
                                m_messageBuffer.clear().append(message.content).trim_end();
                            }

                            if (ImGui::BeginTable("#logs", 1, ImGuiTableFlags_RowBg))
                            {
                                constexpr f32 cellOffset = 8.f;

                                ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_None);

                                ImGui::TableNextRow();
                                ImGui::TableSetColumnIndex(0);

                                ImGui::BeginGroup();

                                ImGui::SameLine(cellOffset);

                                const auto linesCount =
                                    std::count(m_messageBuffer.begin(), m_messageBuffer.end(), '\n');

                                const auto height =
                                    ImGui::GetTextLineHeightWithSpacing() + ImGui::GetTextLineHeight() * linesCount;

                                ImGui::InputTextMultiline("",
                                    m_messageBuffer.mutable_data().data(),
                                    m_messageBuffer.size(),
                                    {800, height},
                                    ImGuiInputTextFlags_ReadOnly);

                                ImGui::NewLine();
                                ImGui::SameLine(cellOffset);

                                if (ImGui::Button(ICON_FA_COPY))
                                {
                                    ImGui::SetClipboardText(message.content.c_str());
                                }

                                ImGui::SameLine();

                                ImGui::EndGroup();

                                draw_severity_accent(message.severity);

                                ImGui::EndTable();
                            }

                            ImGui::EndPopup();
                        }

                        ImGui::PopStyleVar();
                    }
                }

                if (m_autoScroll)
                {
                    ImGui::SetScrollHereY(1.f);
                }

                ImGui::EndTable();
            }
        }

        ImGui::End();

        return open;
    }
}