#pragma once

#include <oblo/ecs/forward.hpp>
#include <oblo/editor/services/editor_world.hpp>
#include <oblo/editor/services/selected_entities.hpp>
#include <oblo/editor/services/update_dispatcher.hpp>
#include <oblo/runtime/runtime.hpp>

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
        runtime m_scene;
        editor_world m_editorWorld;
        update_subscriptions* m_updateSubscriptions{};
        h32<update_subscriber> m_subscription{};
        time m_lastFrameTime{};
    };
}