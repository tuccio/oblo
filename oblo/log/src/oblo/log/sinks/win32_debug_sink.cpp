#ifdef WIN32

    #include <oblo/log/sinks/win32_debug_sink.hpp>

    #include <oblo/core/platform/core.hpp>
    #include <oblo/log/log_internal.hpp>

namespace oblo::log
{
    void win32_debug_sink::sink(severity severity, cstring_view message)
    {
        const auto severityString = get_severity_string(severity);

        platform::debug_output(severityString.c_str());
        platform::debug_output(message.c_str());
        platform::debug_output("\n");
    }
}

#endif