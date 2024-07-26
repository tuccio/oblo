#ifdef _WIN32

    #include <oblo/core/debug.hpp>
    #include <oblo/core/filesystem/filesystem.hpp>
    #include <oblo/core/platform/core.hpp>
    #include <oblo/core/platform/platform_win32.hpp>
    #include <oblo/core/platform/shell.hpp>

    #include <utf8cpp/utf8.h>

    #if defined(WIN32)
        #define NOMINMAX
        #include <Windows.h>

        #include <ShlObj.h>
        #include <ShlObj_core.h>
    #endif

namespace oblo::platform
{
    namespace
    {
        HMODULE g_moduleHandle{};
    }

    bool init()
    {
        g_moduleHandle = GetModuleHandle(nullptr);

        const auto res = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        return res == S_OK || res == S_FALSE || res == RPC_E_CHANGED_MODE;
    }

    void shutdown() {}

    void debug_output(const char* str)
    {
        OutputDebugStringA(str);
    }

    bool is_debugger_attached()
    {
        return IsDebuggerPresent() == TRUE;
    }

    void wait_for_attached_debugger()
    {
        while (!is_debugger_attached())
        {
        }
    }

    void open_folder(string_view dir)
    {
        wchar_t buffer[MAX_PATH];
        win32::convert_path(dir, buffer);

        [[maybe_unused]] const auto res = ShellExecuteW(nullptr, L"explore", buffer, nullptr, nullptr, SW_SHOWDEFAULT);
        OBLO_ASSERT(res != 0);
    }

    bool open_file_dialog(string_builder& file)
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
            file.clear().append(reinterpret_cast<const char16_t*>(szFile));
            return true;
        }

        return false;
    }

    bool search_program_files(string_builder& out, string_view relativePath)
    {
        wchar_t path[MAX_PATH];

        if (SHGetSpecialFolderPathW(0, path, CSIDL_PROGRAM_FILES, FALSE) == TRUE)
        {
            out.clear().append(reinterpret_cast<const char16_t*>(path)).append_path(relativePath);

            if (filesystem::exists(out))
            {
                return true;
            }
        }

        return false;
    }

    void* find_symbol(const char* name)
    {
        return reinterpret_cast<void*>(GetProcAddress(g_moduleHandle, name));
    }
}

#endif