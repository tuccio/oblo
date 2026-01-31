#pragma once

#include <oblo/core/filesystem/file_ptr.hpp>
#include <oblo/log/log_sink.hpp>

namespace oblo::log
{
    class OBLO_LOG_API file_sink : public log_sink
    {
    public:
        explicit file_sink(filesystem::file_ptr ptr);
        explicit file_sink(FILE* ptr);

        file_sink(const file_sink&) = delete;
        file_sink(file_sink&&) = delete;

        file_sink& operator=(const file_sink&) = delete;
        file_sink& operator=(file_sink&&) = delete;

        ~file_sink();

        void set_base_time(time baseTime);

        void sink(severity severity, time timestmap, cstring_view message) override;

    private:
        FILE* m_file{};
        bool m_isOwned{};
        time m_baseTime{};
    };
}