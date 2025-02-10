#pragma once

#include <oblo/input/input_queue.hpp>

namespace oblo
{
    struct window_event_dispatcher
    {
        void (*dispatch)(const void* event);
    };

    class window_event_processor
    {
    public:
        window_event_processor() = default;

        window_event_processor(const window_event_processor&) = default;
        window_event_processor& operator=(const window_event_processor&) = default;

        void set_event_dispatcher(const window_event_dispatcher& dispatcher);
        void set_input_queue(input_queue* inputQueue);

        bool process_events() const;

    private:
        input_queue* m_inputQueue{};
        window_event_dispatcher m_windowEventDispatcher{};
    };
}