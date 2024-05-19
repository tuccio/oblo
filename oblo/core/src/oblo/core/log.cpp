#include <oblo/core/log.hpp>
#include <oblo/core/platform/core.hpp>
#include <oblo/core/utility.hpp>

#include <cstdio>
#include <string_view>

namespace oblo::log::detail
{
    namespace
    {
        constexpr bool logToStderr{true};

        constexpr std::string_view severityStrings[]{
            "[DEBUG] ",
            "[INFO] ",
            "[WARN] ",
            "[ERROR] ",
        };
    }

    void sink_it(severity severity, char* str, usize n)
    {
        const auto severityString{severityStrings[u32(severity)]};

#if defined(WIN32)
        // Make sure it's null-terminated
        const auto last = min(detail::MaxLogMessageLength, n);
        str[last] = '\0';

        platform::debug_output(severityString.data());
        platform::debug_output(str);
        platform::debug_output("\n");
#endif

        if constexpr (logToStderr)
        {
            std::fwrite(severityString.data(), 1, severityString.size(), stderr);
            std::fwrite(str, 1, n, stderr);
            std::fputc('\n', stderr);
        }
    }
}