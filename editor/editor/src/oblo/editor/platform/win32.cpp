#ifdef _WIN32

#include <oblo/core/debug.hpp>
#include <oblo/editor/platform/init.hpp>
#include <oblo/editor/platform/shell.hpp>

#include <Windows.h>

namespace oblo::editor::platform
{
    bool init()
    {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        return true;
    }

    void shutdown() {}

    void open_folder(const std::filesystem::path& dir)
    {
        const auto res = ShellExecuteW(nullptr, L"explore", dir.native().c_str(), nullptr, nullptr, SW_SHOWDEFAULT);
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
            .Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST,

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