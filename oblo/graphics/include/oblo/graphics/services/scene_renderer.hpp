#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/handle_flat_pool_set.hpp>
#include <oblo/renderer/graph/frame_graph.hpp>
#include <oblo/renderer/graph/frame_graph_registry.hpp>

#include <span>

namespace oblo::ecs
{
    class entity_registry;
}

namespace oblo
{
    template <typename>
    class resource_ptr;

    class draw_registry;
    class service_registry;
    class texture;
    struct light_config;
    struct light_data;
    struct skybox_settings;
    struct rtao_config;
    struct surfels_gi_config;

    struct scene_lights
    {
        std::span<const light_data> data;
    };

    enum class scene_view_kind
    {
        runtime,
        editor,
    };

    class scene_renderer
    {
    public:
        explicit scene_renderer(frame_graph& frameGraph, draw_registry& drawRegistry);
        scene_renderer(const scene_renderer&) = delete;
        scene_renderer(scene_renderer&&) noexcept = delete;

        ~scene_renderer();

        scene_renderer& operator=(const scene_renderer&) = delete;
        scene_renderer& operator=(scene_renderer&&) noexcept = delete;

        frame_graph& get_frame_graph() const;
        const frame_graph_registry& get_frame_graph_registry() const;

        void ensure_setup(ecs::entity_registry& entityRegistry);

        void setup_lights(const scene_lights& lights);
        void setup_skybox(const resource_ptr<texture>& skybox, const skybox_settings& settings);
        void setup_surfels_gi(const surfels_gi_config& giConfig);
        void setup_rtao(const rtao_config& rtaoConfig);

        h32<frame_graph_subgraph> create_scene_view(scene_view_kind kind);
        void remove_scene_view(h32<frame_graph_subgraph> subgraph);

        std::span<const h32<frame_graph_subgraph>> get_scene_views() const;

        bool is_scene_view(h32<frame_graph_subgraph> graph) const;

        h32<frame_graph_subgraph> get_scene_data_provider() const;

    private:
        struct shadow_graph;

    private:
        frame_graph& m_frameGraph;
        frame_graph_registry m_nodeRegistry;
        draw_registry& m_drawRegistry;
        h32<frame_graph_subgraph> m_sceneDataProvider{};
        h32<frame_graph_subgraph> m_surfelsGI{};
        h32_flat_extpool_dense_set<frame_graph_subgraph> m_sceneViews;
    };
}