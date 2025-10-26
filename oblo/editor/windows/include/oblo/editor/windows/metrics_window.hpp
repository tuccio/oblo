#pragma once

#include <oblo/core/unique_ptr.hpp>

namespace oblo::editor
{
    struct window_update_context;

    class metrics_window final
    {
    public:
        metrics_window();
        metrics_window(const metrics_window&) = delete;
        metrics_window(metrics_window&&) = delete;
        ~metrics_window();

        bool init(const window_update_context&);
        bool update(const window_update_context&);

    private:
        struct impl;
        unique_ptr<impl> m_impl;
    };
}