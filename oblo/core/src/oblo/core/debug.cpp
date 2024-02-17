#include <oblo/core/debug.hpp>

#include <oblo/core/platform/core.hpp>

#if !defined(_DEBUG) || !defined(WIN32)
#include <cstdio>

namespace oblo
{
    void debug_assert_report(const char* filename, int lineNumber, const char* message)
    {
        fprintf(stderr, "[Assert Failed] [%s:%d] %s\n", filename, lineNumber, message);

        OBLO_DEBUGBREAK();
    }
}

#else
#include <crtdbg.h>

namespace oblo
{
    void debug_assert_report(const char* filename, int lineNumber, const char* message)
    {
        if (platform::is_debugger_attached())
        {
            OBLO_DEBUGBREAK();
        }

        _CrtDbgReport(_CRT_ASSERT, filename, lineNumber, "oblo", message);
    }
}

#endif