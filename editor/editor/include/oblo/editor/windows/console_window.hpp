#pragma once

#include <oblo/core/types.hpp>

namespace oblo::editor
{
    class log_queue;
    struct window_update_context;

    class console_window final
    {
    public:
        void init(const window_update_context&);
        bool update(const window_update_context&);

    private:
        const log_queue* m_logQueue{};
        usize m_selected{~usize{}};
    };
}