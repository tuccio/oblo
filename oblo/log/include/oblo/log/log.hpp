#pragma once

#include <oblo/core/string/format.hpp>
#include <oblo/core/time/clock.hpp>
#include <oblo/core/types.hpp>

#include <format>

namespace oblo::log
{
    enum class severity : u8
    {
        debug,
        info,
        warn,
        error
    };

    namespace detail
    {
        static constexpr usize MaxLogMessageLength{1023u};
        LOG_API void sink_it(severity severity, time t, char* str, usize n);
    }

    template <typename... Args>
    void generic(severity severity, std::format_string<Args...> formatString, Args&&... args)
    {
        char buffer[detail::MaxLogMessageLength + 1];

        const auto endIt =
            std::format_to_n(buffer, detail::MaxLogMessageLength, formatString, std::forward<Args>(args)...);

        detail::sink_it(severity, clock::now(), buffer, usize(endIt.out - buffer));
    }

    template <typename... Args>
    void debug(std::format_string<Args...> formatString, Args&&... args)
    {
        generic(severity::debug, formatString, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void info(std::format_string<Args...> formatString, Args&&... args)
    {
        generic(severity::info, formatString, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void warn(std::format_string<Args...> formatString, Args&&... args)
    {
        generic(severity::warn, formatString, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void error(std::format_string<Args...> formatString, Args&&... args)
    {
        generic(severity::error, formatString, std::forward<Args>(args)...);
    }
}