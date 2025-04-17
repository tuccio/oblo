#pragma once

#include <oblo/ecs/forward.hpp>
#include <oblo/editor/data/time_stats.hpp>
#include <oblo/editor/services/selected_entities.hpp>
#include <oblo/runtime/runtime.hpp>

namespace oblo
{
    class data_document;
}

namespace oblo::editor
{
    class update_subscriptions;
    class world_factory;

    struct update_subscriber;

    struct window_update_context;

    class scene_editing_window final
    {
    public:
        ~scene_editing_window();

        bool init(const window_update_context& ctx);
        bool update(const window_update_context& ctx);
        void on_close();

        ecs::entity_registry& get_entity_registry() const;

    private:
        selected_entities m_selection;
        world_factory* m_worldFactory{};
        update_subscriptions* m_updateSubscriptions{};
        h32<update_subscriber> m_subscription{};
        runtime m_scene;
        time_stats m_timeStats{};
        time m_lastFrameTime{};
    };
}