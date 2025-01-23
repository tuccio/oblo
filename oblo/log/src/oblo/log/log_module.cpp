#include <oblo/log/log_module.hpp>

#include <oblo/core/platform/core.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/log/log.hpp>
#include <oblo/log/log_internal.hpp>
#include <oblo/thread/job_manager.hpp>

#include <moodycamel/concurrentqueue.h>

#include <cstdio>

namespace oblo::log
{
    namespace
    {
        struct message_buffer
        {
            static message_buffer* allocate()
            {
                // It might make allocations faster to use a multiple of the page size
                static_assert(sizeof(message_buffer) == 1024);
                return new message_buffer;
            }

            static void deallocate(message_buffer* b)
            {
                delete b;
            }

            time time;
            u16 len;
            severity severity;
            char data[detail::MaxLogMessageLength];
        };

        struct async_queues
        {
            async_queues() = default;

            moodycamel::ConcurrentQueue<message_buffer*> buffers;
            moodycamel::ConcurrentQueue<message_buffer*> logs;
        };

        unique_ptr<async_queues> g_asyncQueues;
        std::atomic<bool> g_stopFlushJob{};
        job_handle g_flushJob{};

        void sink_it_now(severity severity, time t, char* str, usize n)
        {
            // Make sure it's null-terminated
            const auto last = min(detail::MaxLogMessageLength, n);
            str[last] = '\0';

            const cstring_view message{str, last};

            for (auto& storage : g_logSinks)
            {
                storage.sink->sink(severity, t, message);
            }
        }
    }

    bool log_module::startup(const module_initializer&)
    {
        auto* const jm = job_manager::get();

        // Maybe we should have an option for the async logger
        if (jm)
        {
            g_stopFlushJob = false;
            g_isAsync = true;

            g_asyncQueues = allocate_unique<async_queues>();

            g_flushJob = jm->push_waitable(
                []
                {
                    constexpr u32 n = 32;
                    message_buffer* buffers[n];

                    while (!g_stopFlushJob)
                    {
                        const auto count = g_asyncQueues->logs.try_dequeue_bulk(buffers, n);

                        for (usize i = 0; i < count; ++i)
                        {
                            auto* const buf = buffers[i];
                            sink_it_now(buf->severity, buf->time, buf->data, buf->len);
                        }

                        if (count > 0)
                        {
                            g_asyncQueues->buffers.enqueue_bulk(buffers, count);
                        }

                        // Could use a condition variable instead
                        std::this_thread::yield();
                    }
                });
        }

        return true;
    }

    void log_module::shutdown()
    {
        if (g_isAsync)
        {
            g_stopFlushJob = true;
            job_manager::get()->wait(g_flushJob);
            g_flushJob = {};

            if (g_asyncQueues)
            {
                for (auto* const queue : {&g_asyncQueues->buffers, &g_asyncQueues->logs})
                {
                    message_buffer* buf;

                    while (queue->try_dequeue(buf))
                    {
                        message_buffer::deallocate(buf);
                    }
                }

                g_asyncQueues.reset();
            }
        }

        g_logSinks.clear();
        g_logSinks.shrink_to_fit();
    }

    void log_module::add_sink(unique_ptr<log_sink> sink)
    {
        auto& storage = g_logSinks.emplace_back();
        storage.sink = std::move(sink);
    }

    namespace detail
    {
        void sink_it(severity severity, time t, char* str, usize n)
        {
            if (g_isAsync)
            {
                message_buffer* buf;

                if (!g_asyncQueues->buffers.try_dequeue(buf))
                {
                    buf = message_buffer::allocate();
                }

                const auto len = min(detail::MaxLogMessageLength, n);

                std::memcpy(buf->data, str, len);
                buf->severity = severity;
                buf->time = t;
                buf->len = u16(len);

                g_asyncQueues->logs.enqueue(buf);
            }
            else
            {
                sink_it_now(severity, t, str, n);
            }
        }
    }
}