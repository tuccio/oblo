#pragma once

#include <oblo/core/handle.hpp>
#include <oblo/core/string/string_view.hpp>

namespace oblo
{
    struct frame_graph_subgraph;

    class graphics_window_context
    {
    public:
        virtual ~graphics_window_context() = default;

        virtual void on_visibility_change(bool visible) = 0;
        virtual void on_resize(u32 width, u32 height) = 0;
        virtual void on_destroy() = 0;

        virtual h32<frame_graph_subgraph> get_swapchain_graph() const = 0;

        virtual void set_output(h32<frame_graph_subgraph> sg, string_view name) = 0;
    };
}