#include <oblo/core/debug.hpp>

#if !defined(_DEBUG) || !defined(WIN32)
#include <cstdio>

namespace oblo
{
    void debug_assert_report(const char* filename, int lineNumber, const char* moduleName, const char* message)
    {
        fprintf(stderr,
                "[Assert Failed] [%s:%d]%s%s %s",
                filename,
                lineNumber,
                moduleName ? " " : "",
                moduleName ? moduleName : "",
                message);

        OBLO_DEBUGBREAK();
    }
}

#else
#include <crtdbg.h>

namespace oblo
{
    void debug_assert_report(const char* filename, int lineNumber, const char* moduleName, const char* message)
    {
        _CrtDbgReport(_CRT_ASSERT, filename, lineNumber, moduleName, message);
    }
}

#endif