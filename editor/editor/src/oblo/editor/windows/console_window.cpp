#include <oblo/editor/windows/console_window.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/core/flags.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/utility.hpp>
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

    class console_window::state
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

            incremental_parse(messages);

            if (needsRebuild)
            {
                rebuild(messages);
            }
            else
            {
                incremental_filter(messages);
            }
        }

        bool has_filter() const
        {
            return m_hasFilter;
        }

        usize get_filtered_count() const
        {
            return m_filteredIndices.size();
        }

        usize get_filtered_index(usize i) const
        {
            return m_filteredIndices[i];
        }

        struct clip_result
        {
            usize begin;
            usize end;
            int dy;
        };

        clip_result clip_items_by_y(int pixelMin, int pixelMax) const
        {
            const auto& linesPrefixSum = has_filter() ? m_filteredLinesPrefixSum : m_linesPrefixSum;

            const auto l = std::lower_bound(linesPrefixSum.begin(), linesPrefixSum.end(), usize(pixelMin));
            const auto u = std::upper_bound(linesPrefixSum.begin(), linesPrefixSum.end(), usize(pixelMax));

            const auto first = l - linesPrefixSum.begin();
            const auto last = u - linesPrefixSum.begin();

            // linesPrefixSum[i] = minY of ith message
            // linesPrefixSum[i+1] = maxY of ith message

            // If maxY is the lower_bound, we need to render message i
            const auto b = usize(first > 0 ? first - 1 : 0);

            // We keep end at i+1, where i is the last message we want ot render
            const auto e = min(usize(last), linesPrefixSum.size() - 1);

            // We calculate the dy based on b.minY (the minY of the first message to render)
            // We may need to partially clip the first message when scrolling, so we need to move the cursor up slightly
            const int dy = pixelMin - int(linesPrefixSum[b]);

            return {b, e, dy};
        }

        usize get_max_height() const
        {
            const auto& linesPrefixSum = has_filter() ? m_filteredLinesPrefixSum : m_linesPrefixSum;
            return linesPrefixSum.back();
        }

    private:
        void rebuild(const deque<log_queue::message>& messages)
        {
            m_lastFilteredMessage = 0;
            m_filteredIndices.clear();
            m_hasFilter = m_impl.IsActive() || !m_disabledSeverities.is_empty();
            m_filteredLinesPrefixSum = {0u};
            incremental_filter(messages);
        }

        void incremental_filter(const deque<log_queue::message>& messages)
        {
            if (!has_filter())
            {
                return;
            }

            const auto& g = *ImGui::GetCurrentContext();
            usize accum = m_filteredLinesPrefixSum.back();

            for (; m_lastFilteredMessage < messages.size(); ++m_lastFilteredMessage)
            {
                const auto& msg = messages[m_lastFilteredMessage];

                if (!m_disabledSeverities.contains(msg.severity) &&
                    m_impl.PassFilter(msg.content.begin(), msg.content.end()))
                {
                    m_filteredIndices.emplace_back(m_lastFilteredMessage);

                    const ImVec2 textSize = ImGui::CalcTextSize(msg.content.begin(), msg.content.end());
                    const auto lineHeight = 2 * g.Style.CellPadding.y + textSize.y;

                    accum += usize(lineHeight);

                    m_filteredLinesPrefixSum.push_back(accum);
                }
            }
        }

        void incremental_parse(const deque<log_queue::message>& messages)
        {
            const auto& g = *ImGui::GetCurrentContext();

            usize accum = m_linesPrefixSum.back();
            auto lastParsedMessage = m_linesPrefixSum.size() - 1;

            for (; lastParsedMessage < messages.size(); ++lastParsedMessage)
            {
                const auto& msg = messages[lastParsedMessage];

                const ImVec2 textSize = ImGui::CalcTextSize(msg.content.begin(), msg.content.end());
                const auto lineHeight = 2 * g.Style.CellPadding.y + textSize.y;

                accum += usize(lineHeight);

                // m_linesPrefixSum[i] = minY of ith message
                // m_linesPrefixSum[i+1] = maxY of ith message
                m_linesPrefixSum.push_back(accum);
            }
        }

    private:
        ImGuiTextFilter m_impl{};
        usize m_lastFilteredMessage{};
        deque<usize> m_filteredIndices;
        deque<usize> m_filteredLinesPrefixSum;
        // Initializes the first element to 0 because we keep track of min width and height
        deque<usize> m_linesPrefixSum{get_global_allocator(), 1};
        flags<log::severity, g_severityCount> m_disabledSeverities{};
        bool m_hasFilter{};
    };

    console_window::console_window() = default;

    console_window::~console_window() = default;

    void console_window::init(const window_update_context& ctx)
    {
        m_logQueue = ctx.services.find<const log_queue>();
        OBLO_ASSERT(m_logQueue);

        m_state = std::make_unique<state>();
    }

    bool console_window::update(const window_update_context&)
    {
        bool open{true};

        if (ImGui::Begin("Console", &open, ImGuiWindowFlags_NoScrollbar))
        {
            const auto& messages = m_logQueue->get_messages();

            m_state->update(messages);

            ImGui::SameLine();

            const bool autoScrollSetByUser = ui::toggle_button(ICON_FA_ARROWS_DOWN_TO_LINE, &m_autoScroll);
            ImGui::SetItemTooltip("Toggle auto-scroll");

            ImGui::SetItemTooltip("Copy selected message to clipboard");

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

                const auto maxClipHeight = m_state->get_max_height();

                ImGuiListClipper clipper;
                clipper.Begin(int(maxClipHeight), 1.f);

                if (m_autoScroll)
                {
                    clipper.IncludeItemByIndex(int(maxClipHeight));
                }

                while (clipper.Step())
                {
                    const auto [itemBegin, itemEnd, dy] =
                        m_state->clip_items_by_y(clipper.DisplayStart, clipper.DisplayEnd);

                    // Make sure we keep consistency of the alternating table row colors
                    ImGui::GetCurrentTable()->RowBgColorCounter = int(itemBegin);

                    for (usize itemIndex = itemBegin; itemIndex < itemEnd; ++itemIndex)
                    {
                        usize messageIndex = itemIndex;

                        if (m_state->has_filter())
                        {
                            messageIndex = m_state->get_filtered_index(itemIndex);
                            OBLO_ASSERT(messageIndex < messages.size() && messageIndex >= 0);
                        }

                        const auto& message = messages[usize(messageIndex)];

                        ImGui::TableNextRow();

                        ImGui::TableSetColumnIndex(0);

                        // NOTE: We only have 1 column, but this needs to be done for each column if we dd more
                        if (itemIndex == itemBegin)
                        {
                            const f32 cursorY = ImGui::GetCursorPosY();
                            const f32 scrollY = ImGui::GetScrollY();
                            const f32 dScroll = scrollY - dy;
                            ImGui::SetCursorPosY(cursorY - dScroll);
                        }

                        draw_message(message.severity, message.content);

                        const auto selectableHeight = ImGui::GetItemRectSize().y;

                        ImGui::SameLine();

                        if (ImGui::Selectable(buf.clear().format("##item{}", messageIndex).c_str(),
                                m_selected == messageIndex,
                                ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick,
                                {0, selectableHeight}))
                        {
                            m_selected = messageIndex;
                        }

                        if (ImGui::BeginPopupContextItem(buf.clear().format("##ctx{}", messageIndex).c_str()))
                        {
                            if (ImGui::MenuItem("Copy to clipboard"))
                            {
                                ImGui::SetClipboardText(messages[messageIndex].content.c_str());
                            }

                            ImGui::EndPopup();
                        }

                        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{});

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