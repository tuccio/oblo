#include <oblo/editor/services/log_queue.hpp>

#include <oblo/log/log.hpp>

namespace oblo::editor
{
    struct log_queue::buffer
    {
        char data[log::detail::MaxLogMessageLength + 1];
    };

    constexpr usize MessagesPerDequeChunk{1024};

    log_queue::log_queue() :
        m_stringAllocator{get_global_allocator(), deque_config{.elementsPerChunk = MessagesPerDequeChunk}},
        m_pushQueue{get_global_allocator(), deque_config{.elementsPerChunk = MessagesPerDequeChunk}},
        m_messages{get_global_allocator(), deque_config{.elementsPerChunk = MessagesPerDequeChunk}}
    {
    }

    log_queue::~log_queue() = default;

    void log_queue::push(log::severity severity, time timestamp, cstring_view message)
    {
        auto& newMessage = m_stringAllocator.push_back_default();
        std::memcpy(newMessage.data, message.data(), message.size() + 1);

        std::scoped_lock lock{m_mutex};
        m_pushQueue.emplace_back(severity, timestamp, cstring_view{newMessage.data, message.size()});
    }

    void log_queue::flush()
    {
        std::scoped_lock lock{m_mutex};
        m_messages.append(m_pushQueue.begin(), m_pushQueue.end());
        m_pushQueue.clear();
    }
}