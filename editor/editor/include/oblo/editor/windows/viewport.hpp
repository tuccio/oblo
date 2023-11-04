#pragma once

#include <oblo/ecs/handles.hpp>

namespace oblo::ecs
{
    class entity_registry;
}

namespace oblo::editor
{
    struct window_update_context;

    class viewport final
    {
    public:
        void init(const window_update_context& ctx);
        bool update(const window_update_context& ctx);

    private:
        ecs::entity_registry* m_entities{};
        ecs::entity m_entity{};
    };
}