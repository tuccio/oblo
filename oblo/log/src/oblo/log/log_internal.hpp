#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/unique_ptr.hpp>
#include <oblo/log/log_sink.hpp>

namespace oblo::log
{
    inline dynamic_array<unique_ptr<log_sink>> g_logSinks;

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