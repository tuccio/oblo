#include <oblo/graphics/systems/lighting_system.hpp>

#include <oblo/core/buffered_array.hpp>
#include <oblo/core/frame_allocator.hpp>
#include <oblo/core/iterator/zip_range.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/range.hpp>
#include <oblo/ecs/systems/system_update_context.hpp>
#include <oblo/ecs/utility/registration.hpp>
#include <oblo/graphics/components/light_component.hpp>
#include <oblo/graphics/systems/scene_renderer.hpp>
#include <oblo/math/quaternion.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/options/options_module.hpp>
#include <oblo/scene/components/global_transform_component.hpp>
#include <oblo/scene/components/tags.hpp>
#include <oblo/scene/utility/ecs_utility.hpp>
#include <oblo/vulkan/data/blur_configs.hpp>
#include <oblo/vulkan/data/light_data.hpp>
#include <oblo/vulkan/data/raytraced_shadow_config.hpp>
#include <oblo/vulkan/graph/frame_graph.hpp>
#include <oblo/vulkan/templates/graph_templates.hpp>

namespace oblo
{
    struct lighting_system::shadow_directional
    {
        // Maps main view to shadow map graph
        h32_flat_extpool_dense_map<vk::frame_graph_subgraph, h32<vk::frame_graph_subgraph>> shadowGraphs;
        i32 lightIndex;
        const light_component* light;
    };

    lighting_system::lighting_system() = default;

    lighting_system::~lighting_system() = default;

    void lighting_system::first_update(const ecs::system_update_context& ctx)
    {
        m_sceneRenderer = ctx.services->find<scene_renderer>();
        OBLO_ASSERT(m_sceneRenderer);

        auto* optionsModule = module_manager::get().find<options_module>();
        OBLO_ASSERT(optionsModule);

        m_optionsManager = &optionsModule->manager();

        m_giOptions.init(*m_optionsManager);

        m_sceneRenderer->ensure_setup();

        // Hacky setup for directional light
        const auto e = ecs_utility::create_named_physical_entity<light_component, transient_tag>(*ctx.entities,
            "Sun",
            {},
            quaternion::from_euler_xyz_intrinsic(degrees_tag{}, vec3{.x = -53.f, .y = -8.f, .z = -32.f}),
            vec3::splat(1.f));

        ctx.entities->get<light_component>(e) = {
            .type = light_type::directional,
            .color = vec3::splat(1.f),
            .intensity = 10.f,
            .isShadowCaster = true,
            .shadowSamples = 4,
            .shadowBias = .01f,
            .shadowTemporalAccumulationFactor = .3f,
            .shadowBlurKernel = 5,
            .shadowBlurSigma = 1.15f,
        };

        m_rtShadows = vk::raytraced_shadow_view::create(m_sceneRenderer->get_frame_graph_registry());

        update(ctx);
    }

    void lighting_system::update(const ecs::system_update_context& ctx)
    {
        m_sceneRenderer->setup_surfels_gi(m_giOptions.maxSurfels.read(*m_optionsManager),
            m_giOptions.gridCellSize.read(*m_optionsManager));

        const auto lightsRange = ctx.entities->range<light_component, global_transform_component>();

        const u32 lightsCount = lightsRange.count();

        dynamic_array<vk::light_data> lightData{ctx.frameAllocator};
        lightData.reserve(lightsCount);

        for (auto& shadow : m_shadows.values())
        {
            shadow.lightIndex = -1;
        }

        for (const auto [entities, lights, transforms] : lightsRange)
        {
            for (const auto& [e, light, transform] : zip_range(entities, lights, transforms))
            {
                const vec4 position = transform.localToWorld.columns[3];
                const vec4 direction = normalize(transform.localToWorld * vec4{.z = -1.f});

                const f32 cosInner = std::cos(light.spotInnerAngle.value);
                const f32 cosOuter = std::cos(light.spotOuterAngle.value);

                f32 angleScale{0.f};
                f32 angleOffset{1.f};

                if (light.type == light_type::spot)
                {
                    angleScale = 1.f / max(.001f, cosInner - cosOuter);
                    angleOffset = -cosOuter * angleScale;
                }

                if (light.isShadowCaster)
                {
                    const auto [it, inserted] = m_shadows.emplace(e);
                    it->lightIndex = narrow_cast<i32>(lightData.size());
                    it->light = &light;
                }

                lightData.push_back({
                    .position = {position.x, position.y, position.z},
                    .invSqrRadius = 1.f / (light.radius * light.radius),
                    .direction = {direction.x, direction.y, direction.z},
                    .type = vk::light_type(u32(light.type)),
                    .intensity = light.color * light.intensity,
                    .lightAngleScale = angleScale,
                    .lightAngleOffset = angleOffset,
                    .shadowBias = light.shadowBias,
                });
            }
        }

        m_sceneRenderer->setup_lights({
            .data = lightData,
        });

        // TODO: (#57) We defer removal because we don't have a way to iterate and delete
        buffered_array<ecs::entity, 8> removedShadows{ctx.frameAllocator};
        buffered_array<h32<vk::frame_graph_subgraph>, 4> lastFrameViews{ctx.frameAllocator};

        auto& frameGraph = m_sceneRenderer->get_frame_graph();

        for (const auto& [e, shadow] : zip_range(m_shadows.keys(), m_shadows.values()))
        {
            if (shadow.lightIndex < 0)
            {
                for (const auto shadowGraph : shadow.shadowGraphs.values())
                {
                    frameGraph.remove(shadowGraph);
                }

                removedShadows.push_back(e);
                shadow.shadowGraphs.clear();
            }
            else
            {
                const auto shadowViews = shadow.shadowGraphs.keys();
                lastFrameViews.assign(shadowViews.begin(), shadowViews.end());

                for (const auto scene : shadowViews)
                {
                    if (!m_sceneRenderer->is_scene_view(scene))
                    {
                        shadow.shadowGraphs.erase(scene);
                    }
                }

                for (auto sceneView : m_sceneRenderer->get_scene_views())
                {
                    auto* v = shadow.shadowGraphs.try_find(sceneView);

                    if (!v)
                    {
                        const auto shadowMappingGraph = frameGraph.instantiate(m_rtShadows);

                        frameGraph.connect(m_sceneRenderer->get_scene_data_provider(),
                            vk::scene_data::OutLightBuffer,
                            shadowMappingGraph,
                            vk::raytraced_shadow_view::InLightBuffer);

                        frameGraph.connect(sceneView,
                            vk::main_view::OutCameraBuffer,
                            shadowMappingGraph,
                            vk::raytraced_shadow_view::InCameraBuffer);

                        frameGraph.connect(sceneView,
                            vk::main_view::OutResolution,
                            shadowMappingGraph,
                            vk::raytraced_shadow_view::InResolution);

                        frameGraph.connect(sceneView,
                            vk::main_view::OutDepthBuffer,
                            shadowMappingGraph,
                            vk::raytraced_shadow_view::InDepthBuffer);

                        frameGraph.connect(sceneView,
                            vk::main_view::OutVisibilityBuffer,
                            shadowMappingGraph,
                            vk::raytraced_shadow_view::InVisibilityBuffer);

                        frameGraph.connect(shadowMappingGraph,
                            vk::raytraced_shadow_view::OutShadowSink,
                            sceneView,
                            vk::main_view::InShadowSink);

                        const auto sceneDataProvider = m_sceneRenderer->get_scene_data_provider();

                        frameGraph.connect(sceneDataProvider,
                            vk::scene_data::OutInstanceTables,
                            shadowMappingGraph,
                            vk::raytraced_shadow_view::InInstanceTables);
                        frameGraph.connect(sceneDataProvider,
                            vk::scene_data::OutInstanceBuffers,
                            shadowMappingGraph,
                            vk::raytraced_shadow_view::InInstanceBuffers);

                        frameGraph.connect(sceneDataProvider,
                            vk::scene_data::OutMeshDatabase,
                            shadowMappingGraph,
                            vk::raytraced_shadow_view::InMeshDatabase);

                        const auto [it, ok] = shadow.shadowGraphs.emplace(sceneView, shadowMappingGraph);

                        v = &*it;
                    }

                    const vk::raytraced_shadow_config cfg{
                        .shadowSamples = max(1u, shadow.light->shadowSamples),
                        .lightIndex = u32(shadow.lightIndex),
                        .type = vk::light_type(shadow.light->type),
                        .shadowPunctualRadius = shadow.light->shadowPunctualRadius,
                        .temporalAccumulationFactor = shadow.light->shadowTemporalAccumulationFactor,
                        .hardShadows = shadow.light->hardShadows,
                    };

                    frameGraph.set_input(*v, vk::raytraced_shadow_view::InConfig, cfg).assert_value();
                }
            }
        }

        for (auto e : removedShadows)
        {
            m_shadows.erase(e);
        }
    }
}