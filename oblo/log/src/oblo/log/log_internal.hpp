#pragma once

#include <oblo/core/deque.hpp>
#include <oblo/core/unique_ptr.hpp>
#include <oblo/log/log_sink.hpp>

#include <mutex>

namespace oblo::log
{
    struct sink_storage
    {
        unique_ptr<log_sink> sink;
        std::mutex mutex;
    };

    inline deque<sink_storage> g_logSinks;

    constexpr cstring_view g_severityStrings[]{
        "[DEBUG] ",
        "[INFO] ",
        "[WARN] ",
        "[ERROR] ",
    };

    OBLO_FORCEINLINE cstring_view get_severity_string(severity severity)
    {
        return g_severityStrings[u32(severity)];
    }
}