#include <oblo/log/log.hpp>

#include <oblo/core/platform/core.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/log/log_internal.hpp>

#include <cstdio>

namespace oblo::log::detail
{
    void sink_it(severity severity, time t, char* str, usize n)
    {
        // Make sure it's null-terminated
        const auto last = min(detail::MaxLogMessageLength, n);
        str[last] = '\0';

        const cstring_view message{str, last};

        for (const auto& sink : g_logSinks)
        {
            sink->sink(severity, t, message);
        }
    }
}