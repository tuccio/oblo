#pragma once

namespace oblo::ecs
{
    class entity_registry;
}

namespace oblo::editor
{
    class selected_entities;
    struct window_update_context;

    class inspector final
    {
    public:
        void init(const window_update_context&);
        bool update(const window_update_context&);

    private:
        ecs::entity_registry* m_registry{};
        const selected_entities* m_selection{};
    };
}