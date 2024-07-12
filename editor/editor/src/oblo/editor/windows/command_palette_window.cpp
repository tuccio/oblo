#include <oblo/editor/windows/command_palette_window.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/editor/service_context.hpp>
#include <oblo/editor/services/registered_commands.hpp>
#include <oblo/editor/window_update_context.hpp>

#include <format>

#include <imgui.h>

#include <IconsFontAwesome6.h>

namespace oblo::editor
{
    namespace
    {
        const auto PalettePopupId = "command_palette_window";
    }

    void command_palette_window::init(const window_update_context& ctx)
    {
        m_commands = ctx.services.find<registered_commands>();
        m_filter.Clear();
    }

    bool command_palette_window::update(const window_update_context& ctx)
    {
        bool isOpen{true};

        ImGui::OpenPopup(PalettePopupId);

        if (ImGui::BeginPopupModal(PalettePopupId,
                &isOpen,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoSavedSettings))
        {
            auto* const viewport = ImGui::GetWindowViewport();

            const auto center = viewport->GetCenter();
            const f32 width = 800.f;
            const f32 height = 500.f;

            ImGui::SetWindowPos(ImVec2{center.x - width * .5f, center.y - height * .5f});
            ImGui::SetWindowSize({width, height});

            const auto availableWidth = ImGui::GetContentRegionAvail().x;

            ImGui::SetNextItemWidth(availableWidth);

            if (ImGui::IsWindowAppearing())
            {
                ImGui::SetKeyboardFocusHere();
            }

            const auto modified = ImGui::InputTextWithHint("##input",
                ICON_FA_MAGNIFYING_GLASS " Search commands ...",
                m_filter.InputBuf,
                array_size(m_filter.InputBuf));

            if (modified)
            {
                m_filter.Build();
            }
            else if (!ImGui::IsAnyItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Escape))
            {
                isOpen = false;
            }

            if (ImGui::BeginListBox("##commands", ImGui::GetContentRegionAvail()))
            {
                char buffer[64];

                for (auto& spawnCommands : m_commands->spawnEntityCommands)
                {
                    if (!m_filter.PassFilter(spawnCommands.name))
                    {
                        continue;
                    }

                    auto [last, n] =
                        std::format_to_n(buffer, array_size(buffer), "##{}", std::bit_cast<intptr>(&spawnCommands));

                    *last = '\0';

                    if (ImGui::Selectable(buffer))
                    {
                        spawnCommands.spawn(*ctx.services.find<ecs::entity_registry>());
                        isOpen = false;
                    }

                    ImGui::SameLine();
                    ImGui::TextUnformatted(spawnCommands.icon);
                    ImGui::SameLine();
                    ImGui::TextUnformatted(spawnCommands.name);
                }

                ImGui::EndListBox();
            }

            ImGui::EndPopup();
        }

        return isOpen && ImGui::IsPopupOpen(PalettePopupId);
    }
}