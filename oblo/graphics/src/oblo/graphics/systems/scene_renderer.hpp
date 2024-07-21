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
}

namespace oblo
{
    class service_registry;

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

        void add_scene_view(h32<vk::frame_graph_subgraph> subgraph);
        void remove_scene_view(h32<vk::frame_graph_subgraph> subgraph);

        std::span<const h32<vk::frame_graph_subgraph>> get_scene_views() const;

        bool is_scene_view(h32<vk::frame_graph_subgraph> graph) const;

    private:
        struct shadow_graph;

    private:
        vk::frame_graph& m_frameGraph;
        vk::frame_graph_registry m_nodeRegistry;
        h32<vk::frame_graph_subgraph> m_sceneDataProvider{};
        // TODO: (#8) This could be a set
        h32_flat_extpool_dense_map<vk::frame_graph_subgraph, bool> m_sceneViews;
        h32_flat_extpool_dense_map<vk::light_data, shadow_graph> m_shadowCastingGraphs;
    };
}