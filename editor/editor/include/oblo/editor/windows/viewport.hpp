#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/ecs/forward.hpp>
#include <oblo/editor/data/time_stats.hpp>
#include <oblo/editor/utility/gizmo_handler.hpp>
#include <oblo/input/utility/fps_camera_controller.hpp>
#include <oblo/math/vec3.hpp>

#include <string>

namespace oblo
{
    class input_queue;
    class resource_registry;
}

namespace oblo::editor
{
    class selected_entities;
    struct window_update_context;

    class viewport final
    {
    public:
        ~viewport();

        void init(const window_update_context& ctx);
        bool update(const window_update_context& ctx);

        vec3 get_spawn_location() const;

    private:
        void on_close();

    private:
        resource_registry* m_resources{};
        ecs::entity_registry* m_entities{};
        selected_entities* m_selection{};
        const input_queue* m_inputQueue{};
        ecs::entity m_entity{};
        fps_camera_controller m_cameraController;
        u32 m_viewportId{};
        gizmo_handler m_gizmoHandler{};
        const time_stats* m_timeStats{};
        dynamic_array<std::string> m_viewportModes;
    };
}