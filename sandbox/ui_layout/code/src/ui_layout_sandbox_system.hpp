#pragma once

#include <oblo/app/graphics_app.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/ecs/forward.hpp>
#include <oblo/input/input_queue.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/renderer/graph/frame_graph_registry.hpp>
#include <oblo/renderer/graph/frame_graph_template.hpp>
#include <oblo/ui/ui.hpp>

#include <ui_layout_atlas_cache.hpp>
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

        ui::context m_ui;
        input_queue m_inputQueue;

        ui_atlas_cache m_atlasCache;

        bool m_sidebarChecked[6]{};

        bool m_active{};
    };
}