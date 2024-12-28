#pragma once

#include <oblo/core/filesystem/file_ptr.hpp>
#include <oblo/log/log_sink.hpp>

namespace oblo::log
{
    class file_sink : public log_sink
    {
    public:
        LOG_API explicit file_sink(filesystem::file_ptr ptr);
        LOG_API explicit file_sink(FILE* ptr);

        file_sink(const file_sink&) = delete;
        file_sink(file_sink&&) = delete;

        file_sink& operator=(const file_sink&) = delete;
        file_sink& operator=(file_sink&&) = delete;

        ~file_sink();

        LOG_API void set_base_time(time baseTime);

        LOG_API void sink(severity severity, time timestmap, cstring_view message) override;

    private:
        FILE* m_file{};
        bool m_isOwned{};
        time m_baseTime{};
    };
}