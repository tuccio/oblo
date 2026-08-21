#pragma once

#include <oblo/app/graphics_app.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/ecs/forward.hpp>
#include <oblo/renderer/graph/frame_graph_registry.hpp>
#include <oblo/renderer/graph/frame_graph_template.hpp>

#include <ui_layout_render_node.hpp>

namespace oblo
{
    class renderer;

    class ui_layout_sandbox_system
    {
    public:
        ui_layout_sandbox_system();
        ~ui_layout_sandbox_system();

        ui_layout_sandbox_system(const ui_layout_sandbox_system&) = delete;
        ui_layout_sandbox_system(ui_layout_sandbox_system&&) noexcept = delete;
        ui_layout_sandbox_system& operator=(const ui_layout_sandbox_system&) = delete;
        ui_layout_sandbox_system& operator=(ui_layout_sandbox_system&&) noexcept = delete;

        void first_update(const ecs::system_update_context& ctx);
        void update(const ecs::system_update_context& ctx);

    private:
        renderer* m_renderer{};
        u32 m_frameIndex{};

        frame_graph_registry m_nodeRegistry;
        frame_graph_template m_graphTemplate;

        graphics_app m_app;
        h32<frame_graph_subgraph> m_graph{};
        dynamic_array<ui_layout_element_gpu> m_elements;

        bool m_active{};
    };
}