#pragma once

#include <oblo/core/string_interner.hpp>
#include <oblo/render_graph/render_graph.hpp>
#include <oblo/render_graph/render_graph_seq_executor.hpp>
#include <oblo/vulkan/render_pass_manager.hpp>
#include <renderer/renderer_context.hpp>

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
        struct state;

    private:
        string_interner m_stringInterner;
        render_pass_manager m_renderPassManager;

        render_graph m_graph;
        render_graph_seq_executor m_executor;
        renderer_state m_state;
    };
}