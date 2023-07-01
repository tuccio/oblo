#include <renderer/renderer.hpp>

#include <oblo/render_graph/render_graph_builder.hpp>
#include <renderer/nodes/deferred.hpp>
#include <renderer/renderer_context.hpp>
#include <sandbox/context.hpp>

namespace oblo::vk
{
    struct renderer::state
    {
    };

    bool renderer::init(const sandbox_init_context& context)
    {
        const auto ec = render_graph_builder<renderer_context>{}
                            .add_node<deferred_gbuffer_node>()
                            .add_node<deferred_lighting_node>()
                            // .add_edge(&deferred_gbuffer_node::gbuffer, &deferred_lighting_node::gbuffer)
                            .add_edge(&deferred_gbuffer_node::test, &deferred_lighting_node::test)
                            .add_input<allocated_buffer>("camera")
                            .build(m_graph, m_executor);

        if (ec)
        {
            return false;
        }

        m_state = {};

        renderer_context rendererContext{.initContext = &context, .state = m_state};
        return m_executor.initialize(&rendererContext);
    }

    void renderer::shutdown(const sandbox_shutdown_context& context)
    {
        renderer_context rendererContext{.shutdownContext = &context, .state = m_state};
        m_executor.shutdown(&rendererContext);
    }

    void renderer::update(const sandbox_render_context& context)
    {
        renderer_context rendererContext{.renderContext = &context, .state = m_state};
        m_executor.execute(&rendererContext);

        m_state.lastFrameHeight = context.height;
        m_state.lastFrameWidth = context.width;
    }

    void renderer::update_imgui(const sandbox_update_imgui_context&) {}
}