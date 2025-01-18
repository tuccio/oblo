#include <oblo/core/debug.hpp>

#include <oblo/core/platform/core.hpp>

#include <cstdio>

#if defined(_DEBUG) && defined(WIN32)
    #include <crtdbg.h>
#endif

namespace oblo
{
    namespace
    {
        void debug_assert_report_msg(const char* filename, int lineNumber, const char* message)
        {
            fprintf(stderr, "[Assert Failed] [%s:%d] %s\n", filename, lineNumber, message);
        }

#if defined(_DEBUG) && defined(WIN32)
        int __cdecl debug_assert_hook(int nReportType, char* szMsg, int* pnRet)
        {
            const bool isHijacked = nReportType == _CRT_ASSERT || nReportType == _CRT_ERROR;

            if (isHijacked)
            {
                debug_assert_report("CRT Assert", 0, szMsg);
            }

            if (pnRet)
            {
                *pnRet = int{isHijacked};
            }

            return int{isHijacked};
        }
#endif
    }

    void debug_assert_hook_install()
    {
#if defined(_DEBUG) && defined(WIN32)
        _CrtSetReportHook2(_CRT_RPTHOOK_INSTALL, debug_assert_hook);
#endif
    }

    void debug_assert_hook_remove()
    {
#if defined(_DEBUG) && defined(WIN32)
        _CrtSetReportHook2(_CRT_RPTHOOK_REMOVE, debug_assert_hook);
#endif
    }

    void debug_assert_report(const char* filename, int lineNumber, const char* message)
    {
        debug_assert_report_msg(filename, lineNumber, message);

        if (platform::is_debugger_attached())
        {
            OBLO_DEBUGBREAK();
        }

#if defined(_DEBUG) && defined(WIN32)
        _CrtDbgReport(_CRT_ASSERT, filename, lineNumber, "oblo", message);
#endif
    }
}
