#pragma once

#include <oblo/core/uuid.hpp>
#include <oblo/ecs/forward.hpp>
#include <oblo/editor/services/editor_world.hpp>
#include <oblo/editor/services/selected_entities.hpp>
#include <oblo/editor/services/update_dispatcher.hpp>
#include <oblo/runtime/runtime.hpp>
#include <oblo/scene/resources/entity_hierarchy.hpp>

namespace oblo
{
    class asset_registry;
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

        expected<> load_scene(asset_registry& assetRegistry, const uuid& assetId);
        expected<> save_scene(asset_registry& assetRegistry) const;

    private:
        void start_simulation();
        void stop_simulation();

        /// @brief Wipes the current scene, copying the source hierarchy into it.
        /// @remarks When the simulation is active, the simulation world will be wiped, leaving the original scene
        /// untouched.
        expected<> copy_current_from(const entity_hierarchy& source);

        /// @brief Copies the current editor scene into the destination.
        /// @remarks When the simulation is active, this method will still copy the original scene, as it appeared
        /// before simulation started.
        expected<> copy_scene_to(entity_hierarchy& destination) const;

        static expected<> copy_to(const ecs::entity_registry& source, entity_hierarchy& destination);

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
        uuid m_assetId{};
    };
}