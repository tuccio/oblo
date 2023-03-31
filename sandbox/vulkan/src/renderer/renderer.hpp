#pragma once

#include <oblo/render_graph/render_graph.hpp>
#include <oblo/render_graph/render_graph_seq_executor.hpp>

namespace oblo::vk
{
    struct sandbox_init_context;
    struct sandbox_shutdown_context;
    struct sandbox_render_context;
    struct sandbox_update_imgui_context;

    class renderer
    {
    public:
        bool init(const sandbox_init_context& context);
        void shutdown(const sandbox_shutdown_context& context);
        void update(const sandbox_render_context& context);
        void update_imgui(const sandbox_update_imgui_context& context);

    private:
        render_graph m_graph;
        render_graph_seq_executor m_executor;
    };
}