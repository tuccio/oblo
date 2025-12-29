#pragma once

#include <oblo/core/forward.hpp>

namespace oblo::platform
{
    bool init();
    void shutdown();

    void debug_output(const char* str);
    bool is_debugger_attached();

    void wait_for_attached_debugger();

    [[nodiscard]] bool read_environment_variable(string_builder& out, cstring_view key);
    [[nodiscard]] bool write_environment_variable(cstring_view key, cstring_view value);

    void split_paths_environment_variable(dynamic_array<string_view>& out, const string_view value);

    consteval bool is_windows() noexcept
    {
#ifdef WIN32
        return true;
#else
        return false;
#endif
    }

    consteval bool is_linux() noexcept
    {
#ifdef __linux__
        return true;
#else
        return false;
#endif
    }

    consteval bool is_unix_like() noexcept
    {
        return is_linux();
    }
}