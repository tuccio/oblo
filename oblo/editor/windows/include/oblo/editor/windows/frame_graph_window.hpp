#pragma once

#include <oblo/core/unique_ptr.hpp>

namespace oblo::editor
{
    struct window_update_context;

    class frame_graph_window final
    {
    public:
        frame_graph_window();
        ~frame_graph_window();

        void init(const window_update_context& ctx);
        bool update(const window_update_context& ctx);

    private:
        struct impl;

    private:
        unique_ptr<impl> m_impl;
    };
}