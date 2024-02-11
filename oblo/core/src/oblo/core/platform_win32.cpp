#ifdef _WIN32

#include <oblo/core/debug.hpp>
#include <oblo/core/platform/core.hpp>
#include <oblo/core/platform/shell.hpp>

#if defined(WIN32)
#define NOMINMAX
#include <Windows.h>
#endif

namespace oblo::platform
{
    bool init()
    {
        const auto res = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        return res == S_OK || res == S_FALSE || res == RPC_E_CHANGED_MODE;
    }

    void shutdown() {}

    void debug_output(const char* str)
    {
        OutputDebugStringA(str);
    }

    void open_folder(const std::filesystem::path& dir)
    {
        [[maybe_unused]] const auto res =
            ShellExecuteW(nullptr, L"explore", dir.native().c_str(), nullptr, nullptr, SW_SHOWDEFAULT);
        OBLO_ASSERT(res != 0);
    }

    bool open_file_dialog(std::filesystem::path& file)
    {
        constexpr auto N{260};
        wchar_t szFile[N]{};

        // Initialize OPENFILENAME
        OPENFILENAMEW ofn{
            .lStructSize = sizeof(OPENFILENAMEW),
            .hwndOwner = nullptr,
            .lpstrFilter = L"All\0*.*\0",
            .nFilterIndex = 1,
            .lpstrFile = szFile,
            .nMaxFile = N,
            .lpstrFileTitle = nullptr,
            .nMaxFileTitle = 0,
            .lpstrInitialDir = nullptr,
            .Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR,

        };

        if (GetOpenFileNameW(&ofn) == TRUE)
        {
            file = szFile;
            return true;
        }

        return false;
    }
}

#endif