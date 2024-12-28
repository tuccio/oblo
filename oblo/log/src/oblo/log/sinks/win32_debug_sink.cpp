#ifdef WIN32

    #include <oblo/log/sinks/win32_debug_sink.hpp>

    #include <oblo/core/platform/core.hpp>
    #include <oblo/core/string/string_builder.hpp>
    #include <oblo/core/time/time.hpp>
    #include <oblo/log/log_internal.hpp>

namespace oblo::log
{
    void win32_debug_sink::set_base_time(time baseTime)
    {
        m_baseTime = baseTime;
    }

    void win32_debug_sink::sink(severity severity, time timestamp, cstring_view message)
    {
        const f32 dt = to_f32_seconds(timestamp - m_baseTime);

        string_builder sb;
        sb.format("[{:.3f}] ", dt);

        platform::debug_output(sb.c_str());

        const auto severityString = get_severity_string(severity);

        platform::debug_output(severityString.c_str());
        platform::debug_output(message.c_str());
        platform::debug_output("\n");
    }
}

#endif