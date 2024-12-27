#pragma once

#ifdef WIN32

    #include <oblo/log/log_sink.hpp>

namespace oblo::log
{
    class win32_debug_sink : public log_sink
    {
    public:
        LOG_API void sink(severity severity, cstring_view message) override;
    };
}

#endif