#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/handle_flat_pool_map.hpp>
#include <oblo/vulkan/graph/frame_graph.hpp>
#include <oblo/vulkan/graph/frame_graph_registry.hpp>

#include <span>

namespace oblo::vk
{
    struct light_config;
    struct light_data;
    struct skybox_settings;
}

namespace oblo
{
    template <typename>
    class resource_ptr;

    class service_registry;

    class texture;

    struct surfels_gi_config;

    struct scene_lights
    {
        std::span<const vk::light_data> data;
    };

    class scene_renderer
    {
    public:
        explicit scene_renderer(vk::frame_graph& frameGraph);
        scene_renderer(const scene_renderer&) = delete;
        scene_renderer(scene_renderer&&) noexcept = delete;

        ~scene_renderer();

        scene_renderer& operator=(const scene_renderer&) = delete;
        scene_renderer& operator=(scene_renderer&&) noexcept = delete;

        vk::frame_graph& get_frame_graph() const;
        const vk::frame_graph_registry& get_frame_graph_registry() const;

        void ensure_setup();

        void setup_lights(const scene_lights& lights);
        void setup_skybox(const resource_ptr<texture>& skybox, const vk::skybox_settings& settings);
        void setup_surfels_gi(const surfels_gi_config& giConfig);

        void add_scene_view(h32<vk::frame_graph_subgraph> subgraph);
        void remove_scene_view(h32<vk::frame_graph_subgraph> subgraph);

        std::span<const h32<vk::frame_graph_subgraph>> get_scene_views() const;

        bool is_scene_view(h32<vk::frame_graph_subgraph> graph) const;

        h32<vk::frame_graph_subgraph> get_scene_data_provider() const;

    private:
        struct shadow_graph;

    private:
        vk::frame_graph& m_frameGraph;
        vk::frame_graph_registry m_nodeRegistry;
        h32<vk::frame_graph_subgraph> m_sceneDataProvider{};
        h32<vk::frame_graph_subgraph> m_surfelsGI{};
        // TODO: (#8) This could be a set
        h32_flat_extpool_dense_map<vk::frame_graph_subgraph, bool> m_sceneViews;
        h32_flat_extpool_dense_map<vk::light_data, shadow_graph> m_shadowCastingGraphs;
    };
}