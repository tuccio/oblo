#pragma once

#ifdef WIN32

    #include <oblo/log/log_sink.hpp>

namespace oblo::log
{
    class win32_debug_sink : public log_sink
    {
    public:
        OBLO_LOG_API void set_base_time(time baseTime);
        OBLO_LOG_API void sink(severity severity, time timestamp, cstring_view message) override;

    private:
        time m_baseTime{};
    };
}

#endif