#include <oblo/core/log.hpp>

#include <cstdio>
#include <string_view>

namespace oblo::log::detail
{
    namespace
    {
        constexpr std::string_view severityStrings[]{
            "[DEBUG] ",
            "[INFO] ",
            "[WARN] ",
            "[ERROR] ",
        };
    }

    void sink_it(severity severity, const char* str, usize n)
    {
        const std::string_view severityString{severityStrings[u32(severity)]};
        std::fwrite(severityString.data(), 1, severityStrings->size(), stderr);
        std::fwrite(str, 1, n, stderr);
        std::fputc('\n', stderr);
    }
}