#include <oblo/log/sinks/file_sink.hpp>

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

    void file_sink::sink(severity severity, cstring_view message)
    {
        const auto severityString = get_severity_string(severity);

        std::fwrite(severityString.data(), 1, severityString.size(), m_file);
        std::fwrite(message.data(), 1, message.size(), m_file);
        std::fputc('\n', m_file);
    }
}