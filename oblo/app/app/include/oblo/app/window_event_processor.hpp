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
        explicit window_event_processor(const window_event_dispatcher& dispatcher = {}) :
            m_windowEventDispatcher{dispatcher}
        {
        }

        void set_input_queue(input_queue* inputQueue);

        bool process_events() const;

    private:
        input_queue* m_inputQueue{};
        window_event_dispatcher m_windowEventDispatcher{};
    };
}