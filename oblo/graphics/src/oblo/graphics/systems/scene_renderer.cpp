#include <oblo/graphics/systems/scene_renderer.hpp>

#include <oblo/core/service_registry.hpp>
#include <oblo/vulkan/templates/graph_templates.hpp>

namespace oblo
{
    namespace
    {
        void connect_scene_data_provider_to_scene_view(
            vk::frame_graph& g, h32<vk::frame_graph_subgraph> sceneDataProvider, h32<vk::frame_graph_subgraph> mainView)
        {
            g.connect(sceneDataProvider, vk::scene_data::OutLightConfig, mainView, vk::main_view::InLightConfig);
            g.connect(sceneDataProvider, vk::scene_data::OutLightData, mainView, vk::main_view::InLightData);

            g.connect(sceneDataProvider, vk::scene_data::OutInstanceTables, mainView, vk::main_view::InInstanceTables);
            g.connect(sceneDataProvider,
                vk::scene_data::OutInstanceBuffers,
                mainView,
                vk::main_view::InInstanceBuffers);
        }
    }

    struct scene_renderer::shadow_graph
    {
        h32<vk::frame_graph_subgraph> sg;
    };

    scene_renderer::scene_renderer(vk::frame_graph& frameGraph) : m_frameGraph{frameGraph}
    {
        m_nodeRegistry = vk::create_frame_graph_registry();
    }

    scene_renderer::~scene_renderer() = default;

    const vk::frame_graph_registry& scene_renderer::get_frame_graph_registry() const
    {
        return m_nodeRegistry;
    }

    void scene_renderer::ensure_setup()
    {
        if (!m_sceneDataProvider)
        {
            const auto provider = vk::scene_data::create(m_nodeRegistry);
            m_sceneDataProvider = m_frameGraph.instantiate(provider);
        }
    }

    void scene_renderer::setup_lights(const scene_lights& lights)
    {
        m_frameGraph.set_input(m_sceneDataProvider, vk::scene_data::InLightData, lights.data).assert_value();
    }

    void scene_renderer::add_scene_view(h32<vk::frame_graph_subgraph> subgraph)
    {
        m_sceneViews.emplace(subgraph);

        if (m_sceneDataProvider)
        {
            connect_scene_data_provider_to_scene_view(m_frameGraph, m_sceneDataProvider, subgraph);
        }
    }

    void scene_renderer::remove_scene_view(h32<vk::frame_graph_subgraph> subgraph)
    {
        m_sceneViews.erase(subgraph);
    }
}