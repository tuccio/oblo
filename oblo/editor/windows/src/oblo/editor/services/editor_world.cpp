#include <oblo/editor/services/editor_world.hpp>

#include <oblo/core/service_registry.hpp>

namespace oblo::editor
{
    void editor_world::switch_world(ecs::entity_registry& entityRegistry, const service_registry& services)
    {
        m_worldSwitchSubscribers.dispatch();

        m_entityRegistry = &entityRegistry;
        m_sceneRenderer = services.find<scene_renderer>();

        m_selectedEntities.clear();
        m_selectedEntities.push_refresh_event();
    }

    void editor_world::detach_world()
    {
        m_worldSwitchSubscribers.dispatch();

        m_entityRegistry = nullptr;
        m_sceneRenderer = nullptr;

        m_selectedEntities.clear();
        m_selectedEntities.push_refresh_event();
    }

    ecs::entity_registry* editor_world::get_entity_registry() const
    {
        return m_entityRegistry;
    }

    scene_renderer* editor_world::get_scene_renderer() const
    {
        return m_sceneRenderer;
    }

    const selected_entities* editor_world::get_selected_entities() const
    {
        return &m_selectedEntities;
    }

    selected_entities* editor_world::get_selected_entities()
    {
        return &m_selectedEntities;
    }

    const time_stats& editor_world::get_time_stats() const
    {
        return m_timeStats;
    }

    void editor_world::set_time_stats(const time_stats& timeStats)
    {
        m_timeStats = timeStats;
    }

    h32<editor_world_switch> editor_world::register_world_switch_callback(std::function<void()> cb)
    {
        return m_worldSwitchSubscribers.subscribe(std::move(cb));
    }

    void editor_world::unregister_world_switch_callback(h32<editor_world_switch> h)
    {
        m_worldSwitchSubscribers.unsubscribe(h);
    }
}