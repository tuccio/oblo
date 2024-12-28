#pragma once

#include <oblo/core/deque.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/time/time.hpp>

namespace oblo::log
{
    enum class severity : u8;
}

namespace oblo::editor
{
    class log_queue
    {
    public:
        struct message
        {
            log::severity severity;
            time timestamp;
            cstring_view content;
        };

    public:
        log_queue();
        log_queue(const log_queue&) = delete;
        log_queue(log_queue&&) = delete;

        ~log_queue();

        log_queue& operator=(const log_queue&) = delete;
        log_queue& operator=(log_queue&&) = delete;

        void push(log::severity severity, time timestamp, cstring_view message);

        const deque<message>& get_messages() const
        {
            return m_messages;
        }

    private:
        struct buffer;

    private:
        deque<buffer> m_stringAllocator;
        deque<message> m_messages;
    };
}