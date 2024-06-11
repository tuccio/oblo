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

    class scene_renderer
    {
    public:
        explicit scene_renderer(vk::frame_graph& frameGraph);
        scene_renderer(const scene_renderer&) = delete;
        scene_renderer(scene_renderer&&) noexcept = delete;

        ~scene_renderer() = default;

        scene_renderer& operator=(const scene_renderer&) = delete;
        scene_renderer& operator=(scene_renderer&&) noexcept = delete;

        const vk::frame_graph_registry& get_frame_graph_registry() const;

        void ensure_setup();

        void set_light_data(std::span<const vk::light_data> data);

        void add_scene_view(h32<vk::frame_graph_subgraph> subgraph);
        void remove_scene_view(h32<vk::frame_graph_subgraph> subgraph);

    private:
        vk::frame_graph& m_frameGraph;
        vk::frame_graph_registry m_nodeRegistry;
        h32<vk::frame_graph_subgraph> m_sceneDataProvider{};
        // TODO: (#8) This could be a set
        h32_flat_extpool_dense_map<vk::frame_graph_subgraph, bool> m_sceneViews;
    };
}