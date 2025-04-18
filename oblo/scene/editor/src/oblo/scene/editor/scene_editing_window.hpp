#pragma once

#include <oblo/ecs/forward.hpp>
#include <oblo/editor/services/editor_world.hpp>
#include <oblo/editor/services/selected_entities.hpp>
#include <oblo/editor/services/update_dispatcher.hpp>
#include <oblo/runtime/runtime.hpp>
#include <oblo/scene/resources/entity_hierarchy.hpp>

namespace oblo
{
    class data_document;
}

namespace oblo::editor
{
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
        void start_simulation(const window_update_context& ctx);
        void stop_simulation(const window_update_context& ctx);

    private:
        enum class editor_mode : u8;

    private:
        runtime m_scene;
        entity_hierarchy m_sceneBackup;
        editor_world m_editorWorld;
        update_subscriptions* m_updateSubscriptions{};
        time m_lastFrameTime{};
        h32<update_subscriber> m_subscription{};
        editor_mode m_editorMode{};
    };
}