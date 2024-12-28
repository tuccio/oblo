#pragma once

#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/time/time.hpp>
#include <oblo/core/types.hpp>

namespace oblo::log
{
    enum class severity : u8;

    class log_sink
    {
    public:
        virtual ~log_sink() = default;

        virtual void sink(severity severity, time timestamp, cstring_view message) = 0;
    };
}