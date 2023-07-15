#include <oblo/vulkan/renderer.hpp>

#include <oblo/render_graph/render_graph_builder.hpp>
#include <oblo/vulkan/error.hpp>
#include <oblo/vulkan/nodes/blit_image_node.hpp>
#include <oblo/vulkan/nodes/deferred.hpp>
#include <oblo/vulkan/nodes/forward.hpp>
#include <oblo/vulkan/renderer_context.hpp>
#include <oblo/vulkan/texture.hpp>

namespace oblo::vk
{
    bool renderer::init(const renderer::initializer& context)
    {
        m_allocator = &context.allocator;
        m_engine = &context.engine;
        m_resourceManager = &context.resourceManager;

        m_dummy = m_resourceManager->create(
            *m_allocator,
            {
                .size = 16u,
                .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                .memoryUsage = memory_usage::gpu_only,
            });

        m_stringInterner.init(64);
        m_renderPassManager.init(m_engine->get_device(), m_stringInterner, m_dummy);

#if 0
        const auto ec = render_graph_builder<renderer_context>{}
                            .add_node<deferred_gbuffer_node>()
                            .add_node<deferred_lighting_node>()
                            .add_node<blit_image_node>()
                            // .add_edge(&deferred_gbuffer_node::gbuffer, &deferred_lighting_node::gbuffer)
                            .add_input<allocated_buffer>("camera")
                            .add_input<h32<texture>>("final_render_target")
                            .connect(&deferred_gbuffer_node::test, &deferred_lighting_node::test)
                            .connect(&deferred_gbuffer_node::test, &blit_image_node::source)
                            .connect_input<h32<texture>>("final_render_target", &blit_image_node::destination)
                            .build(m_graph, m_executor);
#else
        const auto ec = render_graph_builder<renderer_context>{}
                            .add_node<forward_node>()
                            .add_input<h32<texture>>("final_render_target")
                            .connect_input<h32<texture>>("final_render_target", &forward_node::renderTarget)
                            .build(m_graph, m_executor);
#endif

        if (ec)
        {
            return false;
        }

        renderer_context rendererContext{.renderer = *this, .frameAllocator = context.frameAllocator};
        return m_executor.initialize(&rendererContext);
    }

    void renderer::shutdown(frame_allocator& frameAllocator)
    {
        m_meshes.shutdown(*m_allocator, *m_resourceManager);

        renderer_context rendererContext{.renderer = *this, .frameAllocator = frameAllocator};
        m_executor.shutdown(&rendererContext);

        m_renderPassManager.shutdown();
        m_resourceManager->destroy(*m_allocator, m_dummy);
    }

    void renderer::update(const update_context& context)
    {
        // Set-up the graph inputs
        auto* const finalRenderTarget = m_graph.find_input<h32<texture>>("final_render_target");
        OBLO_ASSERT(finalRenderTarget);

        *finalRenderTarget = context.swapchainTexture;

        renderer_context rendererContext{
            .renderer = *this,
            .frameAllocator = context.frameAllocator,
            .commandBuffer = &context.commandBuffer,
        };

        m_executor.execute(&rendererContext);

        m_stagingBuffer.flush();
    }
}