#pragma once

#include <oblo/ecs/forward.hpp>

#include <imgui.h>

namespace oblo::editor
{
    class selected_entities;
    struct registered_commands;
    struct window_update_context;

    class viewport;

    class command_palette_window
    {
    public:
        void init(const window_update_context& ctx);
        bool update(const window_update_context& ctx);

    private:
        registered_commands* m_commands{};
        ImGuiTextFilter m_filter{};

        ecs::entity_registry* m_entities{};
        selected_entities* m_selection{};
    };
}