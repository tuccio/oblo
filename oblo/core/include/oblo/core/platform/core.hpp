#pragma once

namespace oblo
{
    class cstring_view;
    class string_builder;
}

namespace oblo::platform
{
    bool init();
    void shutdown();

    void debug_output(const char* str);
    bool is_debugger_attached();

    void wait_for_attached_debugger();

    [[nodiscard]] bool read_environment_variable(string_builder& out, cstring_view key);
    [[nodiscard]] bool write_environment_variable(cstring_view key, cstring_view value);

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
}