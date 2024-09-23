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
            g.connect(sceneDataProvider, vk::scene_data::OutLightBuffer, mainView, vk::main_view::InLightBuffer);
            g.connect(sceneDataProvider, vk::scene_data::OutLights, mainView, vk::main_view::InLights);

            g.connect(sceneDataProvider, vk::scene_data::OutInstanceTables, mainView, vk::main_view::InInstanceTables);
            g.connect(sceneDataProvider,
                vk::scene_data::OutInstanceBuffers,
                mainView,
                vk::main_view::InInstanceBuffers);

            g.connect(sceneDataProvider, vk::scene_data::OutMeshDatabase, mainView, vk::main_view::InMeshDatabase);
        }

        void connect_surfels_gi_to_scene_view(
            vk::frame_graph& g, h32<vk::frame_graph_subgraph> surfelsGIGlobal, h32<vk::frame_graph_subgraph> mainView)
        {
            g.connect(surfelsGIGlobal,
                vk::surfels_gi::OutLastFrameGrid,
                mainView,
                vk::main_view::InLastFrameSurfelsGrid);

            g.connect(surfelsGIGlobal,
                vk::surfels_gi::OutLastFramePool,
                mainView,
                vk::main_view::InLastFrameSurfelsPool);
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

    vk::frame_graph& scene_renderer::get_frame_graph() const
    {
        return m_frameGraph;
    }

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

        if (!m_surfelsGI)
        {
            const auto gi = vk::surfels_gi::create(m_nodeRegistry);
            m_surfelsGI = m_frameGraph.instantiate(gi);
        }
    }

    void scene_renderer::setup_lights(const scene_lights& lights)
    {
        m_frameGraph.set_input(m_sceneDataProvider, vk::scene_data::InLights, lights.data).assert_value();
    }

    void scene_renderer::add_scene_view(h32<vk::frame_graph_subgraph> subgraph)
    {
        m_sceneViews.emplace(subgraph);

        if (m_sceneDataProvider)
        {
            connect_scene_data_provider_to_scene_view(m_frameGraph, m_sceneDataProvider, subgraph);
        }

        if (m_surfelsGI)
        {
            connect_surfels_gi_to_scene_view(m_frameGraph, m_surfelsGI, subgraph);
        }
    }

    void scene_renderer::remove_scene_view(h32<vk::frame_graph_subgraph> subgraph)
    {
        m_sceneViews.erase(subgraph);
    }

    std::span<const h32<vk::frame_graph_subgraph>> scene_renderer::get_scene_views() const
    {
        return m_sceneViews.keys();
    }

    bool scene_renderer::is_scene_view(h32<vk::frame_graph_subgraph> graph) const
    {
        return m_sceneViews.try_find(graph) != nullptr;
    }

    h32<vk::frame_graph_subgraph> scene_renderer::get_scene_data_provider() const
    {
        return m_sceneDataProvider;
    }
}