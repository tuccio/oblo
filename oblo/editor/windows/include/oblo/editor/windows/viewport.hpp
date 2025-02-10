#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/ecs/forward.hpp>
#include <oblo/editor/data/time_stats.hpp>
#include <oblo/editor/utility/gizmo_handler.hpp>
#include <oblo/graphics/services/scene_renderer.hpp>
#include <oblo/input/utility/fps_camera_controller.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/vulkan/graph/forward.hpp>

namespace oblo
{
    class input_queue;
    class resource_registry;
    struct uuid;
}

namespace oblo::editor
{
    class incremental_id_pool;
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

        void spawn_artifact(const window_update_context& ctx, uuid id);

    private:
        const resource_registry* m_resources{};
        ecs::entity_registry* m_entities{};
        scene_renderer* m_sceneRenderer{};
        selected_entities* m_selection{};
        incremental_id_pool* m_idPool{};
        const input_queue* m_inputQueue{};
        ecs::entity m_entity{};
        h32<vk::frame_graph_subgraph> m_viewGraph{};
        fps_camera_controller m_cameraController;
        u32 m_viewportId{};
        gizmo_handler m_gizmoHandler{};
        const time_stats* m_timeStats{};
        dynamic_array<cstring_view> m_viewportModes;
    };
}