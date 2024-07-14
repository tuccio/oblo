#include <oblo/vulkan/templates/graph_templates.hpp>

#include <oblo/vulkan/graph/frame_graph_registry.hpp>
#include <oblo/vulkan/nodes/copy_texture_node.hpp>
#include <oblo/vulkan/nodes/draw_call_generator.hpp>
#include <oblo/vulkan/nodes/entity_picking.hpp>
#include <oblo/vulkan/nodes/frustum_culling.hpp>
#include <oblo/vulkan/nodes/instance_table_node.hpp>
#include <oblo/vulkan/nodes/light_provider.hpp>
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
            std::string_view outputName)
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

        // Hacky view buffers node
        graph.make_input(viewBuffers, &view_buffers_node::inResolution, InResolution);
        graph.make_input(viewBuffers, &view_buffers_node::inCameraData, InCamera);
        graph.make_input(viewBuffers, &view_buffers_node::inTimeData, InTime);
        graph.make_input(viewBuffers, &view_buffers_node::inInstanceTables, InInstanceTables);
        graph.make_input(viewBuffers, &view_buffers_node::inInstanceBuffers, InInstanceBuffers);
        graph.make_input(viewBuffers, &view_buffers_node::inFinalRenderTarget, InFinalRenderTarget);

        // Visibility shading inputs
        graph.make_input(visibilityLighting, &visibility_lighting::inLightConfig, InLightConfig);
        graph.make_input(visibilityLighting, &visibility_lighting::inLightData, InLightData);

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
            &view_buffers_node::outMeshDatabase,
            visibilityPass,
            &visibility_pass::inMeshDatabase);

        // Connect inputs to visibility lighting
        const auto connectVisibilityShadingPass =
            [&]<typename T>(vk::frame_graph_template_vertex_handle shadingPass, h32<T>)
        {
            graph.connect(viewBuffers, &view_buffers_node::outCameraBuffer, shadingPass, &T::inCameraBuffer);
            graph.connect(viewBuffers, &view_buffers_node::inResolution, shadingPass, &T::inResolution);
            graph.connect(viewBuffers, &view_buffers_node::inInstanceTables, shadingPass, &T::inInstanceTables);
            graph.connect(viewBuffers, &view_buffers_node::inInstanceBuffers, shadingPass, &T::inInstanceBuffers);
            graph.connect(viewBuffers, &view_buffers_node::outMeshDatabase, shadingPass, &T::inMeshDatabase);

            // Connect vsibility buffer
            graph.connect(visibilityPass, &visibility_pass::outVisibilityBuffer, shadingPass, &T::inVisibilityBuffer);
        };

        connectVisibilityShadingPass(visibilityLighting, h32<visibility_lighting>{});
        connectVisibilityShadingPass(visibilityDebug, h32<visibility_debug>{});

        // Copies to the output textures
        add_copy_output(graph, viewBuffers, visibilityLighting, &visibility_lighting::outShadedImage, OutLitImage);
        add_copy_output(graph, viewBuffers, visibilityDebug, &visibility_debug::outShadedImage, OutDebugImage);

        // Culling + draw call generation
        {
            const auto frustumCulling = graph.add_node<frustum_culling>();

            graph.connect(viewBuffers,
                &view_buffers_node::outPerViewBindingTable,
                frustumCulling,
                &frustum_culling::inPerViewBindingTable);

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
                &view_buffers_node::outMeshDatabase,
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

        graph.make_input(lightProvider, &light_provider::inLights, InLightData);
        graph.make_output(lightProvider, &light_provider::outLightConfig, OutLightConfig);
        graph.make_output(lightProvider, &light_provider::outLightData, OutLightData);

        const auto instanceTableNode = graph.add_node<instance_table_node>();
        graph.make_output(instanceTableNode, &instance_table_node::outInstanceTables, OutInstanceTables);
        graph.make_output(instanceTableNode, &instance_table_node::outInstanceBuffers, OutInstanceBuffers);

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
        registry.register_node<frustum_culling>();
        registry.register_node<visibility_pass>();
        registry.register_node<visibility_debug>();
        registry.register_node<visibility_lighting>();
        registry.register_node<draw_call_generator>();
        registry.register_node<entity_picking>();

        // Scene data
        registry.register_node<light_provider>();
        registry.register_node<instance_table_node>();

        return registry;
    }
}