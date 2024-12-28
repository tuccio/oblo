#include <oblo/log/sinks/file_sink.hpp>

#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/time/clock.hpp>
#include <oblo/core/time/time.hpp>
#include <oblo/log/log_internal.hpp>

namespace oblo::log
{
    file_sink::file_sink(filesystem::file_ptr ptr) : m_file{ptr.release()}, m_isOwned{true}
    {
        OBLO_ASSERT(ptr);
    }

    file_sink::file_sink(FILE* ptr) : m_file{ptr}, m_isOwned{false}
    {
        OBLO_ASSERT(ptr);
    }

    file_sink::~file_sink()
    {
        if (m_isOwned)
        {
            filesystem::file_ptr::deleter_type{}(m_file);
        }
    }

    void file_sink::set_base_time(time baseTime)
    {
        m_baseTime = baseTime;
    }

    void file_sink::sink(severity severity, time timestamp, cstring_view message)
    {
        const f32 dt = to_f32_seconds(timestamp - m_baseTime);

        string_builder sb;
        sb.format("[{:.3f}] ", dt);

        std::fwrite(sb.data(), 1, sb.size(), m_file);

        const auto severityString = get_severity_string(severity);
        std::fwrite(severityString.data(), 1, severityString.size(), m_file);

        std::fwrite(message.data(), 1, message.size(), m_file);
        std::fputc('\n', m_file);
    }
}