#pragma once

#include <oblo/core/subscribe/subscriber_dispatcher.hpp>
#include <oblo/editor/data/time_stats.hpp>
#include <oblo/editor/services/selected_entities.hpp>

#include <functional>

namespace oblo
{
    namespace ecs
    {
        class entity_registry;
    }

    class scene_renderer;
    class service_registry;
}

namespace oblo::editor
{
    struct editor_world_switch;

    class editor_world
    {
    public:
        using switch_callback_type = std::function<void()>;

    public:
        void switch_world(ecs::entity_registry& entityRegistry, const service_registry& services);
        void detach_world();

        ecs::entity_registry* get_entity_registry() const;
        scene_renderer* get_scene_renderer() const;
        const selected_entities* get_selected_entities() const;
        selected_entities* get_selected_entities();

        const time_stats& get_time_stats() const;
        void set_time_stats(const time_stats& timeStats);

        h32<editor_world_switch> register_world_switch_callback(std::function<void()> cb);
        void unregister_world_switch_callback(h32<editor_world_switch> h);

    private:
        ecs::entity_registry* m_entityRegistry{};
        scene_renderer* m_sceneRenderer{};
        selected_entities m_selectedEntities;
        time_stats m_timeStats{};
        subscriber_dispatcher<editor_world_switch, switch_callback_type> m_worldSwitchSubscribers;
    };
}
