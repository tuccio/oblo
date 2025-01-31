#include <oblo/vulkan/templates/graph_templates.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/core/iterator/enum_range.hpp>
#include <oblo/vulkan/data/blur_configs.hpp>
#include <oblo/vulkan/graph/frame_graph_registry.hpp>
#include <oblo/vulkan/nodes/debug/raytracing_debug.hpp>
#include <oblo/vulkan/nodes/drawing/draw_call_generator.hpp>
#include <oblo/vulkan/nodes/drawing/frustum_culling.hpp>
#include <oblo/vulkan/nodes/postprocess/blur_nodes.hpp>
#include <oblo/vulkan/nodes/postprocess/tone_mapping_node.hpp>
#include <oblo/vulkan/nodes/providers/ecs_entity_set_provider.hpp>
#include <oblo/vulkan/nodes/providers/instance_table_node.hpp>
#include <oblo/vulkan/nodes/providers/light_provider.hpp>
#include <oblo/vulkan/nodes/providers/skybox_provider.hpp>
#include <oblo/vulkan/nodes/providers/view_buffers_node.hpp>
#include <oblo/vulkan/nodes/providers/view_light_data_provider.hpp>
#include <oblo/vulkan/nodes/shadows/raytraced_shadows.hpp>
#include <oblo/vulkan/nodes/shadows/shadow_filter.hpp>
#include <oblo/vulkan/nodes/shadows/shadow_output.hpp>
#include <oblo/vulkan/nodes/shadows/shadow_temporal.hpp>
#include <oblo/vulkan/nodes/surfels/surfel_debug.hpp>
#include <oblo/vulkan/nodes/surfels/surfel_management.hpp>
#include <oblo/vulkan/nodes/utility/copy_texture_node.hpp>
#include <oblo/vulkan/nodes/utility/entity_picking.hpp>
#include <oblo/vulkan/nodes/visibility/visibility_lighting.hpp>
#include <oblo/vulkan/nodes/visibility/visibility_pass.hpp>

namespace oblo::vk
{
    namespace
    {
        template <typename Source>
        void add_copy_output(vk::frame_graph_template& graph,
            frame_graph_template_vertex_handle rtSource,
            frame_graph_template_vertex_handle source,
            resource<texture>(Source::*from),
            string_view outputName)
        {
            const auto copyFinalTarget = graph.add_node<copy_texture_node>();

            graph.make_output(copyFinalTarget, &copy_texture_node::inTarget, outputName);

            graph.connect(rtSource,
                &view_buffers_node::inFinalRenderTarget,
                copyFinalTarget,
                &copy_texture_node::inTarget);

            graph.connect(source, from, copyFinalTarget, &copy_texture_node::inSource);
        }
    }
}

namespace oblo::vk::main_view
{
    frame_graph_template create(const frame_graph_registry& registry, const config& cfg)
    {
        vk::frame_graph_template graph;

        graph.init(registry);

        const auto viewBuffers = graph.add_node<view_buffers_node>();
        const auto viewLightData = graph.add_node<view_light_data_provider>();
        const auto visibilityPass = graph.add_node<visibility_pass>();
        const auto visibilityLighting = graph.add_node<visibility_lighting>();
        const auto visibilityDebug = graph.add_node<visibility_debug>();

        // Hacky view buffers node
        graph.make_input(viewBuffers, &view_buffers_node::inResolution, InResolution);
        graph.make_input(viewBuffers, &view_buffers_node::inCameraData, InCamera);
        graph.make_input(viewBuffers, &view_buffers_node::inTimeData, InTime);
        graph.make_input(viewBuffers, &view_buffers_node::inInstanceTables, InInstanceTables);
        graph.make_input(viewBuffers, &view_buffers_node::inInstanceBuffers, InInstanceBuffers);
        graph.make_input(viewBuffers, &view_buffers_node::inFinalRenderTarget, InFinalRenderTarget);
        graph.make_input(viewBuffers, &view_buffers_node::inMeshDatabase, InMeshDatabase);

        // View light data node
        graph.make_input(viewLightData, &view_light_data_provider::inLights, InLights);
        graph.make_input(viewLightData, &view_light_data_provider::inLightConfig, InLightConfig);
        graph.make_input(viewLightData, &view_light_data_provider::inLightBuffer, InLightBuffer);
        graph.make_input(viewLightData, &view_light_data_provider::inSkyboxSettingsBuffer, InSkyboxSettingsBuffer);

        graph.make_output(viewBuffers, &view_buffers_node::inResolution, OutResolution);
        graph.make_output(viewBuffers, &view_buffers_node::outCameraBuffer, OutCameraBuffer);
        graph.make_output(viewBuffers, &view_buffers_node::outCameraDataSink, OutCameraDataSink);

        // Visibility pass outputs, useful for other graphs (e.g shadows)
        graph.make_output(visibilityPass, &visibility_pass::outVisibilityBuffer, OutVisibilityBuffer);
        graph.make_output(visibilityPass, &visibility_pass::outDepthBuffer, OutDepthBuffer);

        // Visibility shading inputs
        graph.make_input(visibilityLighting, &visibility_lighting::inShadowSink, InShadowSink);

        graph.make_input(visibilityDebug, &visibility_debug::inDebugMode, InDebugMode);

        // Connect view buffers to visibility pass
        graph.connect(viewBuffers, &view_buffers_node::inResolution, visibilityPass, &visibility_pass::inResolution);

        graph.connect(viewBuffers,
            &view_buffers_node::outCameraBuffer,
            visibilityPass,
            &visibility_pass::inCameraBuffer);

        graph.connect(viewBuffers,
            &view_buffers_node::inInstanceTables,
            visibilityPass,
            &visibility_pass::inInstanceTables);

        graph.connect(viewBuffers,
            &view_buffers_node::inInstanceBuffers,
            visibilityPass,
            &visibility_pass::inInstanceBuffers);

        graph.connect(viewBuffers,
            &view_buffers_node::inMeshDatabase,
            visibilityPass,
            &visibility_pass::inMeshDatabase);

        // Connect inputs to visibility lighting
        const auto connectShadingPass = [&]<typename T>(vk::frame_graph_template_vertex_handle shadingPass, h32<T>)
        {
            graph.connect(viewBuffers, &view_buffers_node::outCameraBuffer, shadingPass, &T::inCameraBuffer);
            graph.connect(viewBuffers, &view_buffers_node::inResolution, shadingPass, &T::inResolution);
            graph.connect(viewBuffers, &view_buffers_node::inInstanceTables, shadingPass, &T::inInstanceTables);
            graph.connect(viewBuffers, &view_buffers_node::inInstanceBuffers, shadingPass, &T::inInstanceBuffers);
            graph.connect(viewBuffers, &view_buffers_node::inMeshDatabase, shadingPass, &T::inMeshDatabase);

            if constexpr (requires { &T::inLights; })
            {
                graph.connect(viewLightData, &view_light_data_provider::inLights, shadingPass, &T::inLights);
            }

            if constexpr (requires { &T::inLightConfig; })
            {
                graph.connect(viewLightData, &view_light_data_provider::inLightConfig, shadingPass, &T::inLightConfig);
            }

            if constexpr (requires { &T::inLightBuffer; })
            {
                graph.connect(viewLightData, &view_light_data_provider::inLightBuffer, shadingPass, &T::inLightBuffer);
            }

            if constexpr (requires { &T::inSkyboxSettingsBuffer; })
            {
                graph.connect(viewLightData,
                    &view_light_data_provider::inSkyboxSettingsBuffer,
                    shadingPass,
                    &T::inSkyboxSettingsBuffer);
            }
        };

        const auto connectVisibilityShadingPass =
            [&]<typename T>(vk::frame_graph_template_vertex_handle shadingPass, h32<T> h)
        {
            connectShadingPass(shadingPass, h);

            // Connect visibility buffer
            graph.connect(visibilityPass, &visibility_pass::outVisibilityBuffer, shadingPass, &T::inVisibilityBuffer);
        };

        connectVisibilityShadingPass(visibilityLighting, h32<visibility_lighting>{});
        connectVisibilityShadingPass(visibilityDebug, h32<visibility_debug>{});

        // Outputs of the main passes
        const auto toneMapping = graph.add_node<tone_mapping_node>();
        graph.connect(visibilityLighting, &visibility_lighting::outShadedImage, toneMapping, &tone_mapping_node::inHDR);

        // Copies to the output textures
        add_copy_output(graph, viewBuffers, toneMapping, &tone_mapping_node::outLDR, OutLitImage);
        add_copy_output(graph, viewBuffers, visibilityDebug, &visibility_debug::outShadedImage, OutDebugImage);

        {
            // Ray-Tracing debug pass outputs HDR, and has its own tone-mapping, which leats to the RT debug output
            const auto raytracingDebug = graph.add_node<raytracing_debug>();

            connectShadingPass(raytracingDebug, h32<raytracing_debug>{});

            const auto rtToneMapping = graph.add_node<tone_mapping_node>();
            graph.connect(raytracingDebug, &raytracing_debug::outShadedImage, rtToneMapping, &tone_mapping_node::inHDR);

            add_copy_output(graph, viewBuffers, rtToneMapping, &tone_mapping_node::outLDR, OutRTDebugImage);
        }

        // Culling + draw call generation
        {
            const auto frustumCulling = graph.add_node<frustum_culling>();

            graph.connect(viewBuffers,
                &view_buffers_node::outCameraBuffer,
                frustumCulling,
                &frustum_culling::inCameraBuffer);

            graph.connect(viewBuffers,
                &view_buffers_node::inMeshDatabase,
                frustumCulling,
                &frustum_culling::inMeshDatabase);

            graph.connect(viewBuffers,
                &view_buffers_node::inInstanceTables,
                frustumCulling,
                &frustum_culling::inInstanceTables);

            graph.connect(viewBuffers,
                &view_buffers_node::inInstanceBuffers,
                frustumCulling,
                &frustum_culling::inInstanceBuffers);

            const auto drawCallGenerator = graph.add_node<draw_call_generator>();

            // We need to read mesh handles from instance data to generate draw calls

            graph.connect(viewBuffers,
                &view_buffers_node::inInstanceTables,
                drawCallGenerator,
                &draw_call_generator::inInstanceTables);

            graph.connect(viewBuffers,
                &view_buffers_node::inInstanceBuffers,
                drawCallGenerator,
                &draw_call_generator::inInstanceBuffers);

            // Connect the draw data

            graph.connect(frustumCulling,
                &frustum_culling::outDrawBufferData,
                drawCallGenerator,
                &draw_call_generator::inDrawBufferData);

            graph.connect(frustumCulling,
                &frustum_culling::outDrawBufferData,
                visibilityPass,
                &visibility_pass::inDrawData);

            graph.connect(drawCallGenerator,
                &draw_call_generator::outDrawCallBuffer,
                visibilityPass,
                &visibility_pass::inDrawCallBuffer);

            graph.connect(viewBuffers,
                &view_buffers_node::inMeshDatabase,
                drawCallGenerator,
                &draw_call_generator::inMeshDatabase);
        }

        // Picking
        if (cfg.withPicking)
        {
            const auto entityPicking = graph.add_node<entity_picking>();

            graph.connect(visibilityPass,
                &visibility_pass::outVisibilityBuffer,
                entityPicking,
                &entity_picking::inVisibilityBuffer);

            graph.connect(viewBuffers,
                &view_buffers_node::inInstanceTables,
                entityPicking,
                &entity_picking::inInstanceTables);

            graph.connect(viewBuffers,
                &view_buffers_node::inInstanceBuffers,
                entityPicking,
                &entity_picking::inInstanceBuffers);

            graph.make_input(entityPicking, &entity_picking::inPickingConfiguration, InPickingConfiguration);

            // This is quite awkward admittedly, but if no output is active the node is culled
            graph.make_output(entityPicking, &entity_picking::outPickingResult, OutPicking);
        }

        // Surfels GI
        if (cfg.withSurfelsGI)
        {
            const auto surfelsTiling = graph.add_node<surfel_tiling>();

            graph.make_input(surfelsTiling, &surfel_tiling::inSurfelsGrid, InLastFrameSurfelsGrid);
            graph.make_input(surfelsTiling, &surfel_tiling::inSurfelsGridData, InLastFrameSurfelsGridData);
            graph.make_input(surfelsTiling, &surfel_tiling::inSurfelsData, InLastFrameSurfelData);
            graph.make_output(surfelsTiling, &surfel_tiling::outTileCoverageSink, OutSurfelsTileCoverageSink);
            graph.make_input(surfelsTiling,
                &surfel_tiling::inLastFrameSurfelsLightingData,
                InLastFrameSurfelsLightingData);

            graph.make_input(visibilityLighting, &visibility_lighting::inSurfelsGrid, InUpdatedSurfelsGrid);
            graph.make_input(visibilityLighting, &visibility_lighting::inSurfelsGridData, InUpdatedSurfelsGridData);
            graph.make_input(visibilityLighting, &visibility_lighting::inSurfelsData, InUpdatedSurfelsData);
            graph.make_input(visibilityLighting,
                &visibility_lighting::inSurfelsLightingData,
                InUpdatedSurfelsLightingData);
            graph.make_input(visibilityLighting,
                &visibility_lighting::inSurfelsLightEstimatorData,
                InUpdatedSurfelsLightEstimatorData);
            graph.make_input(visibilityLighting, &visibility_lighting::inOutSurfelsLastUsage, InSurfelsLastUsage);

            graph.connect(viewBuffers,
                &view_buffers_node::outCameraBuffer,
                surfelsTiling,
                &surfel_tiling::inCameraBuffer);

            graph.connect(visibilityPass,
                &visibility_pass::outVisibilityBuffer,
                surfelsTiling,
                &surfel_tiling::inVisibilityBuffer);

            graph.connect(viewBuffers,
                &view_buffers_node::inInstanceTables,
                surfelsTiling,
                &surfel_tiling::inInstanceTables);

            graph.connect(viewBuffers,
                &view_buffers_node::inInstanceBuffers,
                surfelsTiling,
                &surfel_tiling::inInstanceBuffers);

            graph.connect(viewBuffers,
                &view_buffers_node::inMeshDatabase,
                surfelsTiling,
                &surfel_tiling::inMeshDatabase);

            graph.connect(surfelsTiling,
                &surfel_tiling::inSurfelsGrid,
                visibilityLighting,
                &visibility_lighting::inSurfelsGrid);

            {
                constexpr string_view outputs[] = {
                    OutGISurfelsImage,
                    OutGiSurfelsLightingImage,
                    OutGiSurfelsRayCount,
                    OutGiSurfelsInconsistency,
                };

                static_assert(u32(surfel_debug::mode::enum_max) == array_size(outputs));

                for (auto mode : enum_range<surfel_debug::mode>())
                {
                    const auto surfelsDebug = graph.add_node<surfel_debug>();

                    graph.bind(surfelsDebug, &surfel_debug::inMode, mode);

                    graph.connect(surfelsTiling,
                        &surfel_tiling::inSurfelsGrid,
                        surfelsDebug,
                        &surfel_debug::inSurfelsGrid);

                    graph.connect(surfelsTiling,
                        &surfel_tiling::inSurfelsGridData,
                        surfelsDebug,
                        &surfel_debug::inSurfelsGridData);

                    graph.connect(surfelsTiling,
                        &surfel_tiling::inSurfelsData,
                        surfelsDebug,
                        &surfel_debug::inSurfelsData);

                    graph.connect(surfelsTiling,
                        &surfel_tiling::inMeshDatabase,
                        surfelsDebug,
                        &surfel_debug::inMeshDatabase);

                    graph.connect(surfelsTiling,
                        &surfel_tiling::inInstanceBuffers,
                        surfelsDebug,
                        &surfel_debug::inInstanceBuffers);

                    graph.connect(surfelsTiling,
                        &surfel_tiling::inInstanceTables,
                        surfelsDebug,
                        &surfel_debug::inInstanceTables);

                    graph.connect(viewBuffers,
                        &view_buffers_node::outCameraBuffer,
                        surfelsDebug,
                        &surfel_debug::inCameraBuffer);

                    graph.connect(visibilityPass,
                        &visibility_pass::outVisibilityBuffer,
                        surfelsDebug,
                        &surfel_debug::inVisibilityBuffer);

                    graph.connect(visibilityLighting,
                        &visibility_lighting::inSurfelsLightingData,
                        surfelsDebug,
                        &surfel_debug::inSurfelsLightingData);

                    graph.connect(visibilityLighting,
                        &visibility_lighting::inSurfelsLightEstimatorData,
                        surfelsDebug,
                        &surfel_debug::inSurfelsLightEstimatorData);

                    graph.connect(toneMapping, &tone_mapping_node::outLDR, surfelsDebug, &surfel_debug::inImage);

                    add_copy_output(graph, viewBuffers, surfelsDebug, &surfel_debug::outDebugImage, outputs[u32(mode)]);
                }
            }
        }

        return graph;
    }
}

namespace oblo::vk::scene_data
{
    frame_graph_template create(const frame_graph_registry& registry)
    {
        vk::frame_graph_template graph;

        graph.init(registry);

        const auto lightProvider = graph.add_node<light_provider>();

        graph.make_input(lightProvider, &light_provider::inOutLights, InLights);
        graph.make_output(lightProvider, &light_provider::outLightConfig, OutLightConfig);
        graph.make_output(lightProvider, &light_provider::outLightData, OutLightBuffer);
        graph.make_output(lightProvider, &light_provider::inOutLights, OutLights);

        const auto instanceTableNode = graph.add_node<instance_table_node>();
        graph.make_output(instanceTableNode, &instance_table_node::outInstanceTables, OutInstanceTables);
        graph.make_output(instanceTableNode, &instance_table_node::outInstanceBuffers, OutInstanceBuffers);
        graph.make_output(instanceTableNode, &instance_table_node::outMeshDatabase, OutMeshDatabase);

        const auto ecsEntitySetProvider = graph.add_node<ecs_entity_set_provider>();
        graph.make_output(ecsEntitySetProvider, &ecs_entity_set_provider::outEntitySet, OutEcsEntitySetBuffer);

        const auto skyboxProvider = graph.add_node<skybox_provider>();
        graph.make_input(skyboxProvider, &skybox_provider::inSkyboxResource, InSkyboxResource);
        graph.make_input(skyboxProvider, &skybox_provider::inSkyboxSettings, InSkyboxSettings);
        graph.make_output(skyboxProvider, &skybox_provider::outSkyboxSettingsBuffer, OutSkyboxSettingsBuffer);

        return graph;
    }
}

namespace oblo::vk::raytraced_shadow_view
{
    frame_graph_template create(const frame_graph_registry& registry)
    {
        vk::frame_graph_template graph;

        graph.init(registry);

        const auto shadows = graph.add_node<raytraced_shadows>();
        const auto momentFilterH = graph.add_node<box_blur_h>();
        const auto momentFilterV = graph.add_node<box_blur_v>();
        const auto temporal = graph.add_node<shadow_temporal>();
        const auto filter0 = graph.add_node<shadow_filter>();
        const auto filter1 = graph.add_node<shadow_filter>();
        const auto filter2 = graph.add_node<shadow_filter>();
        const auto output = graph.add_node<shadow_output>();

        constexpr box_blur_config momentFilter{.kernelSize = 17};

        graph.bind(momentFilterH, &box_blur_h::inConfig, momentFilter);
        graph.bind(momentFilterV, &box_blur_v::inConfig, momentFilter);

        graph.bind(momentFilterH, &box_blur_h::outputInPlace, false);
        graph.bind(momentFilterV, &box_blur_v::outputInPlace, false);

        graph.bind(filter0, &shadow_filter::passIndex, 0u);
        graph.bind(filter1, &shadow_filter::passIndex, 1u);
        graph.bind(filter2, &shadow_filter::passIndex, 2u);

        graph.make_input(shadows, &raytraced_shadows::inResolution, InResolution);
        graph.make_input(shadows, &raytraced_shadows::inLightBuffer, InLightBuffer);
        graph.make_input(shadows, &raytraced_shadows::inCameraBuffer, InCameraBuffer);
        graph.make_input(shadows, &raytraced_shadows::inConfig, InConfig);
        graph.make_input(shadows, &raytraced_shadows::inDepthBuffer, InDepthBuffer);

        graph.make_input(temporal, &shadow_temporal::inVisibilityBuffer, InVisibilityBuffer);
        graph.make_input(temporal, &shadow_temporal::inMeshDatabase, InMeshDatabase);
        graph.make_input(temporal, &shadow_temporal::inInstanceBuffers, InInstanceBuffers);
        graph.make_input(temporal, &shadow_temporal::inInstanceTables, InInstanceTables);

        graph.connect(shadows, &raytraced_shadows::outShadow, momentFilterH, &box_blur_h::inSource);
        graph.connect(momentFilterH, &box_blur_h::outBlurred, momentFilterV, &box_blur_v::inSource);

        graph.connect(shadows, &raytraced_shadows::outShadow, temporal, &shadow_temporal::inShadow);
        graph.connect(momentFilterV, &box_blur_v::outBlurred, temporal, &shadow_temporal::inMoments);

        graph.connect(shadows, &raytraced_shadows::inCameraBuffer, temporal, &shadow_temporal::inCameraBuffer);
        graph.connect(shadows, &raytraced_shadows::inCameraBuffer, filter0, &shadow_filter::inCameraBuffer);
        graph.connect(shadows, &raytraced_shadows::inCameraBuffer, filter1, &shadow_filter::inCameraBuffer);
        graph.connect(shadows, &raytraced_shadows::inCameraBuffer, filter2, &shadow_filter::inCameraBuffer);

        graph.connect(temporal, &shadow_temporal::outFiltered, filter0, &shadow_filter::inSource);
        graph.connect(filter0, &shadow_filter::outFiltered, filter1, &shadow_filter::inSource);
        graph.connect(filter1, &shadow_filter::outFiltered, filter2, &shadow_filter::inSource);

        // A little unintuitive, we use the result of the first filter as history for next frame
        // The first filter pass will create the stable texture
        graph.connect(temporal, &shadow_temporal::inHistory, filter0, &shadow_filter::outFiltered);

        graph.connect(shadows, &raytraced_shadows::inConfig, output, &shadow_output::inConfig);

        graph.connect(filter2, &shadow_filter::outFiltered, output, &shadow_output::outShadow);

        graph.make_output(output, &shadow_output::outShadow, OutShadow);
        graph.make_output(output, &shadow_output::outShadowSink, OutShadowSink);

        return graph;
    }

    type_id get_main_view_barrier_source()
    {
        return get_type_id<raytraced_shadows>();
    }

    type_id get_main_view_barrier_target()
    {
        return get_type_id<visibility_lighting>();
    }
}

namespace oblo::vk::surfels_gi
{
    frame_graph_template create(const frame_graph_registry& registry)
    {
        vk::frame_graph_template graph;

        graph.init(registry);

        const auto initializer = graph.add_node<surfel_initializer>();
        const auto spawner = graph.add_node<surfel_spawner>();
        const auto update = graph.add_node<surfel_update>();
        const auto accumulateRays = graph.add_node<surfel_accumulate_raycount>();
        const auto rayTracing = graph.add_node<surfel_raytracing>();

        // Initializer setup
        graph.make_input(initializer, &surfel_initializer::inGridBounds, InGridBounds);
        graph.make_input(initializer, &surfel_initializer::inGridCellSize, InGridCellSize);
        graph.make_input(initializer, &surfel_initializer::inMaxSurfels, InMaxSurfels);

        graph.make_input(spawner, &surfel_spawner::inTileCoverageSink, InTileCoverageSink);

        graph.bind(initializer,
            &surfel_initializer::inGridBounds,
            aabb{
                .min = {.x = -32, .y = -16, .z = -32},
                .max = {.x = 32, .y = 16, .z = 32},
            });

        // We output the surfels from last frame, then each view will contribute potentially spawning surfels
        graph.make_output(initializer, &surfel_initializer::outSurfelsGrid, OutLastFrameGrid);
        graph.make_output(initializer, &surfel_initializer::outSurfelsGridData, OutLastFrameGridData);
        graph.make_output(initializer, &surfel_initializer::outSurfelsData, OutLastFrameSurfelData);
        graph.make_output(initializer, &surfel_initializer::outSurfelsLastUsage, OutSurfelsLastUsage);
        graph.make_output(initializer,
            &surfel_initializer::outLastFrameSurfelsLightingData,
            OutLastFrameSurfelsLightingData);

        graph.bind(initializer, &surfel_initializer::inGridCellSize, .5f);
        graph.bind(initializer, &surfel_initializer::inMaxSurfels, 1u << 16);

        // Spawner setup
        graph.connect(initializer,
            &surfel_initializer::outSurfelsSpawnData,
            spawner,
            &surfel_spawner::inOutSurfelsSpawnData);
        graph.connect(initializer, &surfel_initializer::outSurfelsData, spawner, &surfel_spawner::inOutSurfelsData);
        graph.connect(initializer, &surfel_initializer::outSurfelsStack, spawner, &surfel_spawner::inOutSurfelsStack);
        graph.connect(initializer,
            &surfel_initializer::outSurfelsLastUsage,
            spawner,
            &surfel_spawner::inOutSurfelsLastUsage);
        graph.connect(initializer,
            &surfel_initializer::outLastFrameSurfelsLightingData,
            spawner,
            &surfel_spawner::inOutLastFrameSurfelsLightingData);

        // Update setup
        graph.make_input(update, &surfel_update::inCameras, InCameraDataSink);
        graph.make_input(update, &surfel_update::inInstanceTables, InInstanceTables);
        graph.make_input(update, &surfel_update::inInstanceBuffers, InInstanceBuffers);
        graph.make_input(update, &surfel_update::inMeshDatabase, InMeshDatabase);
        graph.make_input(update, &surfel_update::inEntitySetBuffer, InEcsEntitySetBuffer);

        graph.connect(initializer, &surfel_initializer::inMaxSurfels, update, &surfel_update::inMaxSurfels);
        graph.connect(initializer, &surfel_initializer::outCellsCount, update, &surfel_update::inCellsCount);
        graph.connect(initializer, &surfel_initializer::inGridBounds, update, &surfel_update::inGridBounds);
        graph.connect(initializer, &surfel_initializer::inGridCellSize, update, &surfel_update::inGridCellSize);
        graph.connect(spawner, &surfel_spawner::inOutSurfelsData, update, &surfel_update::inOutSurfelsData);
        graph.connect(spawner, &surfel_spawner::inOutSurfelsStack, update, &surfel_update::inOutSurfelsStack);
        graph.connect(spawner, &surfel_spawner::inOutSurfelsSpawnData, update, &surfel_update::inOutSurfelsSpawnData);
        graph.connect(initializer, &surfel_initializer::outSurfelsGrid, update, &surfel_update::inOutSurfelsGrid);
        graph.connect(initializer,
            &surfel_initializer::outSurfelsGridData,
            update,
            &surfel_update::inOutSurfelsGridData);
        graph.connect(initializer,
            &surfel_initializer::outSurfelsLightEstimatorData,
            update,
            &surfel_update::inSurfelsLightEstimatorData);
        graph.connect(spawner, &surfel_spawner::inOutSurfelsLastUsage, update, &surfel_update::inOutSurfelsLastUsage);

        graph.make_output(update, &surfel_update::inOutSurfelsData, OutUpdatedSurfelData);
        graph.make_output(update, &surfel_update::inOutSurfelsGrid, OutUpdatedSurfelGrid);
        graph.make_output(update, &surfel_update::inOutSurfelsGridData, OutUpdatedSurfelGridData);

        // Accumulate ray count setup
        graph.connect(update,
            &surfel_update::inOutSurfelsData,
            accumulateRays,
            &surfel_accumulate_raycount::inSurfelsData);

        graph.connect(initializer,
            &surfel_initializer::inMaxSurfels,
            accumulateRays,
            &surfel_accumulate_raycount::inMaxSurfels);

        // Ray-Tracing setup
        graph.make_input(rayTracing, &surfel_raytracing::inMaxRayPaths, InMaxRayPaths);
        graph.make_input(rayTracing, &surfel_raytracing::inGIMultiplier, InGIMultiplier);
        graph.make_input(rayTracing, &surfel_raytracing::inLightBuffer, InLightBuffer);
        graph.make_input(rayTracing, &surfel_raytracing::inLightConfig, InLightConfig);
        graph.make_input(rayTracing, &surfel_raytracing::inSkyboxSettingsBuffer, InSkyboxSettingsBuffer);

        graph.bind(rayTracing, &surfel_raytracing::inMaxRayPaths, 1u << 20);
        graph.bind(rayTracing, &surfel_raytracing::inGIMultiplier, 1.f);

        graph.connect(update, &surfel_update::inOutSurfelsGrid, rayTracing, &surfel_raytracing::inOutSurfelsGrid);
        graph.connect(update,
            &surfel_update::inOutSurfelsGridData,
            rayTracing,
            &surfel_raytracing::inOutSurfelsGridData);
        graph.connect(update, &surfel_update::inOutSurfelsData, rayTracing, &surfel_raytracing::inOutSurfelsData);
        graph.connect(update, &surfel_update::inMeshDatabase, rayTracing, &surfel_raytracing::inMeshDatabase);
        graph.connect(update, &surfel_update::inInstanceTables, rayTracing, &surfel_raytracing::inInstanceTables);
        graph.connect(update, &surfel_update::inInstanceBuffers, rayTracing, &surfel_raytracing::inInstanceBuffers);
        graph.connect(initializer, &surfel_initializer::inMaxSurfels, rayTracing, &surfel_raytracing::inMaxSurfels);
        graph.connect(initializer,
            &surfel_initializer::outSurfelsLightEstimatorData,
            rayTracing,
            &surfel_raytracing::inOutSurfelsLightEstimatorData);

        graph.connect(spawner,
            &surfel_spawner::inOutLastFrameSurfelsLightingData,
            rayTracing,
            &surfel_raytracing::inLastFrameSurfelsLightingData);

        graph.connect(initializer,
            &surfel_initializer::outSurfelsLightingData,
            rayTracing,
            &surfel_raytracing::inOutSurfelsLightingData);

        graph.connect(update,
            &surfel_update::inOutSurfelsLastUsage,
            rayTracing,
            &surfel_raytracing::inOutSurfelsLastUsage);

        graph.connect(accumulateRays,
            &surfel_accumulate_raycount::outTotalRayCount,
            rayTracing,
            &surfel_raytracing::inTotalRayCount);

        graph.make_output(rayTracing, &surfel_raytracing::inOutSurfelsLightingData, OutUpdatedSurfelLightingData);
        graph.make_output(rayTracing,
            &surfel_raytracing::inOutSurfelsLightEstimatorData,
            OutUpdatedSurfelLightEstimatorData);
        graph.make_output(rayTracing, &surfel_raytracing::inOutSurfelsLastUsage, OutSurfelsLastUsage);

        return graph;
    }
}

namespace oblo::vk
{
    vk::frame_graph_registry create_frame_graph_registry()
    {
        frame_graph_registry registry;

        // Main view
        registry.register_node<copy_texture_node>();
        registry.register_node<view_buffers_node>();
        registry.register_node<view_light_data_provider>();
        registry.register_node<frustum_culling>();
        registry.register_node<visibility_pass>();
        registry.register_node<visibility_debug>();
        registry.register_node<visibility_lighting>();
        registry.register_node<draw_call_generator>();
        registry.register_node<entity_picking>();
        registry.register_node<raytracing_debug>();
        registry.register_node<tone_mapping_node>();

        // Scene data
        registry.register_node<ecs_entity_set_provider>();
        registry.register_node<light_provider>();
        registry.register_node<instance_table_node>();
        registry.register_node<skybox_provider>();

        // Ray-traced shadows
        registry.register_node<raytraced_shadows>();
        registry.register_node<shadow_output>();
        registry.register_node<shadow_filter>();
        registry.register_node<shadow_temporal>();

        // Blurs
        registry.register_node<gaussian_blur_h>();
        registry.register_node<gaussian_blur_v>();
        registry.register_node<box_blur_h>();
        registry.register_node<box_blur_v>();

        // Surfels GI
        registry.register_node<surfel_initializer>();
        registry.register_node<surfel_tiling>();
        registry.register_node<surfel_spawner>();
        registry.register_node<surfel_update>();
        registry.register_node<surfel_accumulate_raycount>();
        registry.register_node<surfel_raytracing>();
        registry.register_node<surfel_debug>();

        return registry;
    }
}