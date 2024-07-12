#pragma once

#include <oblo/core/types.hpp>

namespace oblo::ecs
{
    class entity_registry;
}

namespace oblo::editor
{
    class selected_entities;
    struct window_update_context;

    class scene_hierarchy final
    {
    public:
        void init(const window_update_context& ctx);
        bool update(const window_update_context& ctx);

    private:
        ecs::entity_registry* m_registry{};
        selected_entities* m_selection{};
        u32 m_lastRefreshEvent{};
    };
}