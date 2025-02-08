#pragma once

#include <oblo/core/handle.hpp>

namespace oblo
{
    namespace vk
    {
        struct frame_graph_subgraph;
    }

    class graphics_window_context
    {
    public:
        virtual ~graphics_window_context() = default;

        virtual void on_resize(u32 width, u32 height) = 0;
        virtual void on_destroy() = 0;

        virtual h32<vk::frame_graph_subgraph> get_swapchain_graph() const = 0;
    };
}