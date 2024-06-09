#include <oblo/vulkan/templates/graph_templates.hpp>

#include <oblo/vulkan/graph/frame_graph_registry.hpp>
#include <oblo/vulkan/nodes/bypass_culling.hpp>
#include <oblo/vulkan/nodes/copy_texture_node.hpp>
#include <oblo/vulkan/nodes/forward_pass.hpp>
#include <oblo/vulkan/nodes/frustum_culling.hpp>
#include <oblo/vulkan/nodes/picking_readback.hpp>
#include <oblo/vulkan/nodes/view_buffers_node.hpp>

namespace oblo::vk::main_view
{
    frame_graph_template create(const frame_graph_registry& registry, const config& cfg)
    {
        vk::frame_graph_template graph;

        graph.init(registry);

        const auto viewBuffers = graph.add_node<view_buffers_node>();
        const auto forwardPass = graph.add_node<forward_pass>();
        const auto copyFinalTarget = graph.add_node<copy_texture_node>();

        graph.make_input(viewBuffers, &view_buffers_node::inCameraData, InCamera);
        graph.make_input(viewBuffers, &view_buffers_node::inTimeData, InTime);

        graph.make_input(forwardPass, &forward_pass::inResolution, InResolution);
        graph.make_input(forwardPass, &forward_pass::inPickingConfiguration, InPickingConfiguration);

        graph.make_input(copyFinalTarget, &copy_texture_node::inTarget, InFinalRenderTarget);

        graph.connect(viewBuffers,
            &view_buffers_node::outPerViewBindingTable,
            forwardPass,
            &forward_pass::inPerViewBindingTable);

        graph.connect(forwardPass, &forward_pass::outRenderTarget, copyFinalTarget, &copy_texture_node::inSource);

        if (cfg.bypassCulling)
        {
            const auto bypassCulling = graph.add_node<bypass_culling>();
            graph.connect(bypassCulling, &bypass_culling::outDrawBufferData, forwardPass, &forward_pass::inDrawData);
        }
        else
        {
            const auto frustumCulling = graph.add_node<frustum_culling>();

            graph.connect(viewBuffers,
                &view_buffers_node::outPerViewBindingTable,
                frustumCulling,
                &frustum_culling::inPerViewBindingTable);

            graph.connect(frustumCulling, &frustum_culling::outDrawBufferData, forwardPass, &forward_pass::inDrawData);
        }

        if (cfg.withPicking)
        {
            const auto pickingReadback = graph.add_node<picking_readback>();

            graph.connect(forwardPass,
                &forward_pass::outPickingIdBuffer,
                pickingReadback,
                &picking_readback::inPickingIdBuffer);

            graph.connect(forwardPass,
                &forward_pass::inPickingConfiguration,
                pickingReadback,
                &picking_readback::inPickingConfiguration);
        }

        return graph;
    }
}

namespace oblo::vk
{
    vk::frame_graph_registry create_frame_graph_registry()
    {
        frame_graph_registry registry;

        registry.register_node<copy_texture_node>();
        registry.register_node<view_buffers_node>();
        registry.register_node<frustum_culling>();
        registry.register_node<forward_pass>();
        registry.register_node<picking_readback>();

        return registry;
    }
}