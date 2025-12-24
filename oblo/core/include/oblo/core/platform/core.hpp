#pragma once

namespace oblo::platform
{
    bool init();
    void shutdown();

    void debug_output(const char* str);
    bool is_debugger_attached();

    void wait_for_attached_debugger();

    void* find_symbol(const char* name);

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