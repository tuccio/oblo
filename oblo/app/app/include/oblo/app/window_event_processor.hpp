#pragma once

namespace oblo
{
    struct window_event_dispatcher
    {
        void (*dispatch)(const void* event);
    };

    class window_event_processor
    {
    public:
        explicit window_event_processor(const window_event_dispatcher& dispatcher) : m_windowEventDispatcher{dispatcher}
        {
        }

        bool process_events() const;

    private:
        window_event_dispatcher m_windowEventDispatcher{};
    };
}