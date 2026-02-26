#include <oblo/graphics/services/scene_renderer.hpp>

#include <oblo/core/service_registry.hpp>
#include <oblo/graphics/systems/graphics_options.hpp>
#include <oblo/math/aabb.hpp>
#include <oblo/renderer/data/render_world.hpp>
#include <oblo/renderer/data/skybox_settings.hpp>
#include <oblo/renderer/templates/graph_templates.hpp>
#include <oblo/resource/resource_ptr.hpp>

namespace oblo
{
    namespace
    {
        void connect_scene_data_provider_to_scene_view(
            frame_graph& g, h32<frame_graph_subgraph> sceneDataProvider, h32<frame_graph_subgraph> mainView)
        {
            g.connect(sceneDataProvider, scene_data::OutRenderWorld, mainView, main_view::InRenderWorld);

            g.connect(sceneDataProvider, scene_data::OutLightConfig, mainView, main_view::InLightConfig);
            g.connect(sceneDataProvider, scene_data::OutLightBuffer, mainView, main_view::InLightBuffer);
            g.connect(sceneDataProvider, scene_data::OutLights, mainView, main_view::InLights);

            g.connect(sceneDataProvider, scene_data::OutInstanceTables, mainView, main_view::InInstanceTables);
            g.connect(sceneDataProvider, scene_data::OutInstanceBuffers, mainView, main_view::InInstanceBuffers);

            g.connect(sceneDataProvider, scene_data::OutMeshDatabase, mainView, main_view::InMeshDatabase);

            g.connect(sceneDataProvider,
                scene_data::OutSkyboxSettingsBuffer,
                mainView,
                main_view::InSkyboxSettingsBuffer);
        }

        void connect_scene_data_provider_to_surfels_gi(
            frame_graph& g, h32<frame_graph_subgraph> sceneDataProvider, h32<frame_graph_subgraph> surfelsGIGlobal)
        {
            g.connect(sceneDataProvider,
                scene_data::OutEcsEntitySetBuffer,
                surfelsGIGlobal,
                surfels_gi::InEcsEntitySetBuffer);

            g.connect(sceneDataProvider, scene_data::OutMeshDatabase, surfelsGIGlobal, surfels_gi::InMeshDatabase);

            g.connect(sceneDataProvider,
                scene_data::OutInstanceBuffers,
                surfelsGIGlobal,
                surfels_gi::InInstanceBuffers);

            g.connect(sceneDataProvider, scene_data::OutInstanceTables, surfelsGIGlobal, surfels_gi::InInstanceTables);

            g.connect(sceneDataProvider,
                scene_data::OutSkyboxSettingsBuffer,
                surfelsGIGlobal,
                surfels_gi::InSkyboxSettingsBuffer);

            g.connect(sceneDataProvider, scene_data::OutLightConfig, surfelsGIGlobal, surfels_gi::InLightConfig);

            g.connect(sceneDataProvider, scene_data::OutLightBuffer, surfelsGIGlobal, surfels_gi::InLightBuffer);
        }

        void connect_surfels_gi_to_scene_view(
            frame_graph& g, h32<frame_graph_subgraph> surfelsGIGlobal, h32<frame_graph_subgraph> mainView)
        {
            g.connect(surfelsGIGlobal, surfels_gi::OutLastFrameGrid, mainView, main_view::InLastFrameSurfelsGrid);

            g.connect(surfelsGIGlobal,
                surfels_gi::OutLastFrameGridData,
                mainView,
                main_view::InLastFrameSurfelsGridData);

            g.connect(surfelsGIGlobal, surfels_gi::OutLastFrameSurfelData, mainView, main_view::InLastFrameSurfelData);

            g.connect(surfelsGIGlobal,
                surfels_gi::OutLastFrameSurfelSpawnData,
                mainView,
                main_view::InLastFrameSurfelSpawnData);

            g.connect(surfelsGIGlobal,
                surfels_gi::OutLastFrameSurfelsLightingData,
                mainView,
                main_view::InLastFrameSurfelsLightingData);

            g.connect(surfelsGIGlobal, surfels_gi::OutUpdatedSurfelGrid, mainView, main_view::InUpdatedSurfelsGrid);

            g.connect(surfelsGIGlobal,
                surfels_gi::OutUpdatedSurfelGridData,
                mainView,
                main_view::InUpdatedSurfelsGridData);

            g.connect(surfelsGIGlobal, surfels_gi::OutUpdatedSurfelData, mainView, main_view::InUpdatedSurfelsData);

            g.connect(surfelsGIGlobal,
                surfels_gi::OutUpdatedSurfelLightingData,
                mainView,
                main_view::InUpdatedSurfelsLightingData);

            g.connect(surfelsGIGlobal,
                surfels_gi::OutUpdatedSurfelLightEstimatorData,
                mainView,
                main_view::InUpdatedSurfelsLightEstimatorData);

            g.connect(surfelsGIGlobal, surfels_gi::OutSurfelsLastUsage, mainView, main_view::InSurfelsLastUsage);

            g.connect(mainView, main_view::OutSurfelsTileCoverageSink, surfelsGIGlobal, surfels_gi::InTileCoverageSink);

            g.connect(mainView, main_view::OutCameraDataSink, surfelsGIGlobal, surfels_gi::InCameraDataSink);
        }
    }

    struct scene_renderer::shadow_graph
    {
        h32<frame_graph_subgraph> sg;
    };

    scene_renderer::scene_renderer(frame_graph& frameGraph, draw_registry& drawRegistry) :
        m_frameGraph{frameGraph}, m_drawRegistry{drawRegistry}
    {
        m_nodeRegistry = create_frame_graph_registry();
    }

    scene_renderer::~scene_renderer()
    {
        if (m_sceneDataProvider)
        {
            m_frameGraph.remove(m_sceneDataProvider);
            m_sceneDataProvider = {};
        }

        if (m_surfelsGI)
        {
            m_frameGraph.remove(m_surfelsGI);
            m_surfelsGI = {};
        }

        for (auto&& view : m_sceneViews.keys())
        {
            m_frameGraph.remove(view);
        }

        m_sceneViews.clear();
    }

    frame_graph& scene_renderer::get_frame_graph() const
    {
        return m_frameGraph;
    }

    const frame_graph_registry& scene_renderer::get_frame_graph_registry() const
    {
        return m_nodeRegistry;
    }

    void scene_renderer::ensure_setup(ecs::entity_registry& entityRegistry)
    {
        if (!m_sceneDataProvider)
        {
            const auto provider = scene_data::create(m_nodeRegistry);
            m_sceneDataProvider = m_frameGraph.instantiate(provider);

            m_frameGraph
                .set_input(m_sceneDataProvider,
                    scene_data::InRenderWorld,
                    render_world{
                        .entityRegistry = &entityRegistry,
                        .drawRegistry = &m_drawRegistry,
                    })
                .assert_value();
        }

        if (!m_surfelsGI)
        {
            const auto gi = surfels_gi::create(m_nodeRegistry);
            m_surfelsGI = m_frameGraph.instantiate(gi);
            connect_scene_data_provider_to_surfels_gi(m_frameGraph, m_sceneDataProvider, m_surfelsGI);
        }
    }

    void scene_renderer::setup_lights(const scene_lights& lights)
    {
        m_frameGraph.set_input(m_sceneDataProvider, scene_data::InLights, lights.data).assert_value();
    }

    void scene_renderer::setup_skybox(const resource_ptr<texture>& skybox, const skybox_settings& settings)
    {
        m_frameGraph.set_input(m_sceneDataProvider, scene_data::InSkyboxResource, skybox).assert_value();
        m_frameGraph.set_input(m_sceneDataProvider, scene_data::InSkyboxSettings, settings).assert_value();
    }

    void scene_renderer::setup_surfels_gi(const surfels_gi_config& giConfig)
    {
        const vec3 gridSize{giConfig.gridSizeX, giConfig.gridSizeY, giConfig.gridSizeZ};

        const auto halfExtents = gridSize * .5f;
        const aabb gridBounds{.min = -halfExtents, .max = halfExtents};

        m_frameGraph.set_input(m_surfelsGI, surfels_gi::InMaxSurfels, giConfig.maxSurfels).assert_value();
        m_frameGraph.set_input(m_surfelsGI, surfels_gi::InMaxRayPaths, giConfig.rayBudget).assert_value();
        m_frameGraph.set_input(m_surfelsGI, surfels_gi::InGridCellSize, giConfig.gridCellSize).assert_value();
        m_frameGraph.set_input(m_surfelsGI, surfels_gi::InGridBounds, gridBounds).assert_value();
        m_frameGraph.set_input(m_surfelsGI, surfels_gi::InGIMultiplier, giConfig.multiplier).assert_value();
    }

    void scene_renderer::setup_rtao(const rtao_config& rtaoConfig)
    {
        for (h32<frame_graph_subgraph> view : m_sceneViews.keys())
        {
            m_frameGraph.set_input(view, main_view::InRTAOBias, rtaoConfig.bias).assert_value();
            m_frameGraph.set_input(view, main_view::InRTAOMaxDistance, rtaoConfig.maxDistance).assert_value();
            m_frameGraph.set_input(view, main_view::InRTAOMaxHistoryWeight, rtaoConfig.maxHistoryWeight).assert_value();
        }
    }

    h32<frame_graph_subgraph> scene_renderer::create_scene_view(scene_view_kind kind)
    {
        const auto mainViewTemplate = main_view::create(m_nodeRegistry,
            {
                .withPicking = kind == scene_view_kind::editor,
            });

        const auto subgraph = m_frameGraph.instantiate(mainViewTemplate);
        m_frameGraph.disable_all_outputs(subgraph);

        m_sceneViews.emplace(subgraph);

        if (m_sceneDataProvider)
        {
            connect_scene_data_provider_to_scene_view(m_frameGraph, m_sceneDataProvider, subgraph);
        }

        if (m_surfelsGI)
        {
            connect_surfels_gi_to_scene_view(m_frameGraph, m_surfelsGI, subgraph);
        }

        return subgraph;
    }

    void scene_renderer::remove_scene_view(h32<frame_graph_subgraph> subgraph)
    {
        m_frameGraph.remove(subgraph);
        m_sceneViews.erase(subgraph);
    }

    std::span<const h32<frame_graph_subgraph>> scene_renderer::get_scene_views() const
    {
        return m_sceneViews.keys();
    }

    bool scene_renderer::is_scene_view(h32<frame_graph_subgraph> graph) const
    {
        return m_sceneViews.try_find(graph) != nullptr;
    }

    h32<frame_graph_subgraph> scene_renderer::get_scene_data_provider() const
    {
        return m_sceneDataProvider;
    }
}