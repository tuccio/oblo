#pragma once

#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/types.hpp>

#include <memory>

namespace oblo::editor
{
    class log_queue;
    struct window_update_context;

    class console_window final
    {
    public:
        console_window();
        console_window(const console_window&) = delete;
        console_window(console_window&&) = delete;
        ~console_window();

        void init(const window_update_context&);
        bool update(const window_update_context&);

    private:
        class filter;

    private:
        const log_queue* m_logQueue{};
        usize m_selected{~usize{}};
        bool m_autoScroll{true};
        std::unique_ptr<filter> m_filter;
        string_builder m_messageBuffer;
    };
}