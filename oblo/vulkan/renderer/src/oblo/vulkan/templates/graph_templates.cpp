#include <oblo/vulkan/templates/graph_templates.hpp>

#include <oblo/vulkan/data/blur_configs.hpp>
#include <oblo/vulkan/graph/frame_graph_registry.hpp>
#include <oblo/vulkan/nodes/blur_nodes.hpp>
#include <oblo/vulkan/nodes/copy_texture_node.hpp>
#include <oblo/vulkan/nodes/draw_call_generator.hpp>
#include <oblo/vulkan/nodes/entity_picking.hpp>
#include <oblo/vulkan/nodes/frustum_culling.hpp>
#include <oblo/vulkan/nodes/instance_table_node.hpp>
#include <oblo/vulkan/nodes/light_provider.hpp>
#include <oblo/vulkan/nodes/raytracing_debug.hpp>
#include <oblo/vulkan/nodes/shadows/raytraced_shadows.hpp>
#include <oblo/vulkan/nodes/shadows/shadow_filter.hpp>
#include <oblo/vulkan/nodes/shadows/shadow_output.hpp>
#include <oblo/vulkan/nodes/shadows/shadow_temporal.hpp>
#include <oblo/vulkan/nodes/surfels/surfel_management.hpp>
#include <oblo/vulkan/nodes/view_buffers_node.hpp>
#include <oblo/vulkan/nodes/visibility_lighting.hpp>
#include <oblo/vulkan/nodes/visibility_pass.hpp>

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
        const auto visibilityPass = graph.add_node<visibility_pass>();
        const auto visibilityLighting = graph.add_node<visibility_lighting>();
        const auto visibilityDebug = graph.add_node<visibility_debug>();
        const auto raytracingDebug = graph.add_node<raytracing_debug>();

        // Hacky view buffers node
        graph.make_input(viewBuffers, &view_buffers_node::inResolution, InResolution);
        graph.make_input(viewBuffers, &view_buffers_node::inCameraData, InCamera);
        graph.make_input(viewBuffers, &view_buffers_node::inTimeData, InTime);
        graph.make_input(viewBuffers, &view_buffers_node::inInstanceTables, InInstanceTables);
        graph.make_input(viewBuffers, &view_buffers_node::inInstanceBuffers, InInstanceBuffers);
        graph.make_input(viewBuffers, &view_buffers_node::inFinalRenderTarget, InFinalRenderTarget);
        graph.make_input(viewBuffers, &view_buffers_node::inMeshDatabase, InMeshDatabase);

        graph.make_output(viewBuffers, &view_buffers_node::inResolution, OutResolution);
        graph.make_output(viewBuffers, &view_buffers_node::outCameraBuffer, OutCameraBuffer);

        // Visibility pass outputs, useful for other graphs (e.g shadows)
        graph.make_output(visibilityPass, &visibility_pass::outVisibilityBuffer, OutVisibilityBuffer);
        graph.make_output(visibilityPass, &visibility_pass::outDepthBuffer, OutDepthBuffer);

        // Visibility shading inputs
        graph.make_input(visibilityLighting, &visibility_lighting::inLights, InLights);
        graph.make_input(visibilityLighting, &visibility_lighting::inLightConfig, InLightConfig);
        graph.make_input(visibilityLighting, &visibility_lighting::inLightBuffer, InLightBuffer);
        graph.make_input(visibilityLighting, &visibility_lighting::inSurfelsGrid, InSurfelsGIGrid);
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
        connectShadingPass(raytracingDebug, h32<raytracing_debug>{});

        // Copies to the output textures
        add_copy_output(graph, viewBuffers, visibilityLighting, &visibility_lighting::outShadedImage, OutLitImage);
        add_copy_output(graph, viewBuffers, visibilityDebug, &visibility_debug::outShadedImage, OutDebugImage);
        add_copy_output(graph, viewBuffers, raytracingDebug, &raytracing_debug::outShadedImage, OutRTDebugImage);

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
            graph.make_output(entityPicking, &entity_picking::outDummyOut, OutPicking);
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
    frame_graph_template create_global(const frame_graph_registry& registry)
    {
        vk::frame_graph_template graph;

        graph.init(registry);

        const auto initializer = graph.add_node<surfel_initializer>();
        const auto spawner = graph.add_node<surfel_spawner>();

        graph.make_input(initializer, &surfel_initializer::inGridBounds, InGridBounds);
        graph.make_input(initializer, &surfel_initializer::inGridCellSize, InGridCellSize);
        graph.make_input(initializer, &surfel_initializer::inMaxSurfels, InMaxSurfels);

        graph.make_output(spawner, &surfel_spawner::inOutSurfelsGrid, OutGrid);

        graph.connect(initializer, &surfel_initializer::outSurfelsGrid, spawner, &surfel_spawner::inOutSurfelsGrid);

        graph.bind(initializer,
            &surfel_initializer::inGridBounds,
            aabb{
                .min = {.x = -256, .y = -32.f, .z = -256},
                .max = {.x = 256, .y = 128.f, .z = 256},
            });

        graph.bind(initializer, &surfel_initializer::inGridCellSize, vec3::splat(2.f));
        graph.bind(initializer, &surfel_initializer::inMaxSurfels, 1u << 14);

        return graph;
    }

    frame_graph_template create_view(const frame_graph_registry& registry);
}

namespace oblo::vk
{
    vk::frame_graph_registry create_frame_graph_registry()
    {
        frame_graph_registry registry;

        // Main view
        registry.register_node<copy_texture_node>();
        registry.register_node<view_buffers_node>();
        registry.register_node<frustum_culling>();
        registry.register_node<visibility_pass>();
        registry.register_node<visibility_debug>();
        registry.register_node<visibility_lighting>();
        registry.register_node<draw_call_generator>();
        registry.register_node<entity_picking>();
        registry.register_node<raytracing_debug>();

        // Scene data
        registry.register_node<light_provider>();
        registry.register_node<instance_table_node>();

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
        registry.register_node<surfel_spawner>();

        return registry;
    }
}