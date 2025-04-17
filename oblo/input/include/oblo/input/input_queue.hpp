#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/input/input_event.hpp>

#include <span>

namespace oblo
{
    using input_events = std::span<const input_event>;

    class input_queue
    {
    public:
        void push(const input_event& e);
        void clear();

        input_events get_events() const;

    private:
        dynamic_array<input_event> m_queue;
    };

    inline void input_queue::push(const input_event& e)
    {
        m_queue.push_back(e);
    }

    inline void input_queue::clear()
    {
        m_queue.clear();
    }

    inline input_events input_queue::get_events() const
    {
        return m_queue;
    }
}