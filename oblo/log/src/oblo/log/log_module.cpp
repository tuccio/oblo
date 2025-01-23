#include <oblo/log/log_module.hpp>

#include <oblo/core/array_size.hpp>
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
            time time;
            u16 len;
            severity severity;
            char data[detail::MaxLogMessageLength];
        };

        struct message_buffer_chunk
        {
            message_buffer buffers[32];
        };

        struct message_buffer_chunk_deleter
        {
            void operator()(message_buffer_chunk* ptr) const
            {
                delete ptr;
            }
        };

        using message_buffer_chunk_ptr = unique_ptr<message_buffer_chunk, message_buffer_chunk_deleter>;

        message_buffer_chunk_ptr message_buffer_chunk_allocate()
        {
            return message_buffer_chunk_ptr{new message_buffer_chunk};
        }

        struct async_queues
        {
            async_queues() = default;

            moodycamel::ConcurrentQueue<message_buffer_chunk_ptr> allocations;
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

            g_asyncQueues.reset();
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

                // Try to get a buffer from the buffer queue first
                if (!g_asyncQueues->buffers.try_dequeue(buf))
                {
                    // Allocate a new chunk, the the first for ourselves and push the rest to the buffers queue
                    auto chunk = message_buffer_chunk_allocate();
                    buf = chunk->buffers;

                    constexpr usize buffersCount = array_size(message_buffer_chunk{}.buffers);
                    static_assert(buffersCount > 2, "This code is a little silly at best with low numbers");

                    // We skip the first, because we take that for ourselves
                    constexpr usize buffersToPush = buffersCount - 1;

                    message_buffer* newBuffers[buffersToPush];

                    for (u32 i = 0; i < buffersToPush; ++i)
                    {
                        newBuffers[i] = &chunk->buffers[i + 1];
                    }

                    g_asyncQueues->buffers.enqueue_bulk(newBuffers, buffersToPush);
                    g_asyncQueues->allocations.enqueue(std::move(chunk));
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