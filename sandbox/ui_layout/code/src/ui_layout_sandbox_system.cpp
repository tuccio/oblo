#include <ui_layout_sandbox_system.hpp>

#include <ui_layout_graph.hpp>
#include <ui_layout_sandbox_layout.hpp>

#include <oblo/app/graphics_app.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/core/time/time.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/ecs/systems/system_update_context.hpp>
#include <oblo/log/log.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/renderer/graph/frame_graph.hpp>
#include <oblo/renderer/renderer.hpp>

#include <span>

namespace oblo
{
    ui_layout_sandbox_system::ui_layout_sandbox_system() = default;

    ui_layout_sandbox_system::~ui_layout_sandbox_system()
    {
        if (m_renderer && m_graph)
        {
            m_renderer->get_frame_graph().remove(m_graph);
        }

        m_app.shutdown();
    }

    void ui_layout_sandbox_system::first_update(const ecs::system_update_context& ctx)
    {
        m_renderer = ctx.services->find<renderer>();
        OBLO_ASSERT(m_renderer);

        m_nodeRegistry.register_node<ui_layout_render_node>();
        m_graphTemplate = ui_layout_view::create(m_nodeRegistry);

        if (!m_app.init({.title = "UI Layout Sandbox", .windowWidth = 1280, .windowHeight = 720}))
        {
            log::error("Failed to create the UI Layout sandbox window");
            return;
        }

        auto& frameGraph = m_renderer->get_frame_graph();

        m_graph = frameGraph.instantiate(m_graphTemplate);
        m_app.set_output(m_graph, ui_layout_view::OutLayoutImage);

        m_active = true;

        sandbox::ui_layout_init();

        update(ctx);
    }

    void ui_layout_sandbox_system::update(const ecs::system_update_context& ctx)
    {
        if (!m_active)
        {
            return;
        }

        if (!m_app.get_main_window().is_open())
        {
            if (m_graph)
            {
                m_renderer->get_frame_graph().remove(m_graph);
                m_graph = {};
            }

            m_app.shutdown();
            m_active = false;
            return;
        }

        auto& frameGraph = m_renderer->get_frame_graph();

        const auto windowSize = m_app.get_main_window().get_size();
        const vec2u resolution{max(windowSize.x, 1u), max(windowSize.y, 1u)};

        sandbox::ui_layout_set_size(vec2{f32(resolution.x), f32(resolution.y)});
        sandbox::ui_layout_update(ctx.dt);

        sandbox::get_ui_layout_elements(m_elements);

        frameGraph.set_input(m_graph, ui_layout_view::InResolution, resolution).assert_value();

        frameGraph.set_input(m_graph, ui_layout_view::InElements, std::span<const ui_layout_element_gpu>{m_elements})
            .assert_value();

        ++m_frameIndex;
    }
}