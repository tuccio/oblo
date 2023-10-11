#include <oblo/graphics/systems/viewport_system.hpp>

#include <oblo/core/service_registry.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/range.hpp>
#include <oblo/ecs/systems/system_update_context.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/graphics/components/viewport_component.hpp>
#include <oblo/vulkan/graph/render_graph.hpp>
#include <oblo/vulkan/graph/render_graph_builder.hpp>
#include <oblo/vulkan/nodes/clear_render_target.hpp>
#include <oblo/vulkan/nodes/forward.hpp>
#include <oblo/vulkan/renderer.hpp>
#include <oblo/vulkan/renderer_context.hpp>
#include <oblo/vulkan/resource_manager.hpp>
#include <oblo/vulkan/stateful_command_buffer.hpp>
#include <oblo/vulkan/texture.hpp>
#include <oblo/vulkan/vulkan_context.hpp>

namespace oblo::graphics
{
    void viewport_system::first_update(const ecs::system_update_context& ctx)
    {
        m_vkCtx = ctx.services->find<vk::vulkan_context>();
        m_renderer = ctx.services->find<vk::renderer>();

        update(ctx);
    }

    void viewport_system::update(const ecs::system_update_context& ctx)
    {
        using namespace oblo::vk;

        for (const auto [entities, viewports] : ctx.entities->range<viewport_component>())
        {
            for (auto& viewport : viewports)
            {
                if (!viewport.texture)
                {
                    continue;
                }

                // // TODO: Removing viewports leaks the graph
                // if (!viewport.renderGraph)
                // {
                //     const auto builder = vk::render_graph_builder<renderer_context>{}
                //                              .add_node<clear_render_target>()
                //                              .add_node<forward_node>()
                //                              .add_input<h32<texture>>("FinalRenderTarget")
                //                              .connect_input("FinalRenderTarget", &clear_render_target::renderTarget)
                //                              .connect(&clear_render_target::renderTarget, &forward_node::renderTarget);

                //     viewport.renderGraph = m_renderer->create_graph(builder, *ctx.frameAllocator);
                // }

                // if (auto* const graph = m_renderer->find_graph(viewport.renderGraph))
                // {
                //     auto* textureInput = graph->find_input<h32<vk::texture>>("FinalRenderTarget");
                //     *textureInput = viewport.texture;
                // }
            }
        }

        // m_renderer->update({.vkContext = *m_vkCtx, .frameAllocator = *ctx.frameAllocator});
    }
}