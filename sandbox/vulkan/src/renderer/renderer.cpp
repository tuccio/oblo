#include <renderer/renderer.hpp>

#include <oblo/render_graph/render_graph_builder.hpp>
#include <renderer/hello_world_node.hpp>
#include <renderer/renderer_context.hpp>

namespace oblo::vk
{
    bool renderer::init(const sandbox_init_context& context)
    {
        const auto ec =
            render_graph_builder<renderer_context>{}.add_node<hello_world_node>().build(m_graph, m_executor);

        if (ec)
        {
            return false;
        }

        renderer_context rendererContext{.initContext = &context};
        return m_executor.initialize(&rendererContext);
    }

    void renderer::shutdown(const sandbox_shutdown_context& context)
    {
        renderer_context rendererContext{.shutdownContext = &context};
        m_executor.shutdown(&rendererContext);
    }

    void renderer::update(const sandbox_render_context& context)
    {
        renderer_context rendererContext{.renderContext = &context};
        m_executor.execute(&rendererContext);
    }

    void renderer::update_imgui(const sandbox_update_imgui_context&) {}
}