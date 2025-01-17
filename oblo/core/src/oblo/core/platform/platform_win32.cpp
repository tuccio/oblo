#ifdef _WIN32

    #include <oblo/core/debug.hpp>
    #include <oblo/core/filesystem/filesystem.hpp>
    #include <oblo/core/platform/core.hpp>
    #include <oblo/core/platform/platform_win32.hpp>
    #include <oblo/core/platform/process.hpp>
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

    namespace
    {
        constexpr u8 g_processProcessHandle = 0;
        constexpr u8 g_processThreadHandle = 1;

        void set_handle(std::span<uintptr, 2> handles, u32 id, HANDLE h)
        {
            handles[id] = std::bit_cast<uintptr>(h);
        }

        HANDLE get_handle(std::span<uintptr, 2> handles, u32 id)
        {
            return std::bit_cast<HANDLE>(handles[id]);
        }
    }

    process::process() = default;

    process::process(process&& other) noexcept
    {
        set_handle(m_handles, g_processProcessHandle, get_handle(other.m_handles, g_processProcessHandle));
        set_handle(m_handles, g_processThreadHandle, get_handle(other.m_handles, g_processThreadHandle));

        set_handle(other.m_handles, g_processProcessHandle, NULL);
        set_handle(other.m_handles, g_processThreadHandle, NULL);
    }

    process::~process()
    {
        detach();
    }

    process& process::operator=(process&& other) noexcept
    {
        detach();

        set_handle(m_handles, g_processProcessHandle, get_handle(other.m_handles, g_processProcessHandle));
        set_handle(m_handles, g_processThreadHandle, get_handle(other.m_handles, g_processThreadHandle));

        set_handle(other.m_handles, g_processProcessHandle, NULL);
        set_handle(other.m_handles, g_processThreadHandle, NULL);

        return *this;
    }

    expected<> process::start(cstring_view path, std::span<const cstring_view> arguments)
    {
        detach();

        constexpr DWORD flags = CREATE_NO_WINDOW;

        STARTUPINFO startupInfo{
            .cb = sizeof(STARTUPINFO),
        };

        PROCESS_INFORMATION processInfo{};

        string_builder cmd;

        cmd.append(path);
        cmd.append(" ");
        cmd.join(arguments.begin(), arguments.end(), " ", "\"{}\"");

        const auto success = CreateProcessA(nullptr,
            cmd.mutable_data().data(),
            nullptr,
            nullptr,
            TRUE,
            flags,
            nullptr,
            nullptr,
            &startupInfo,
            &processInfo);

        if (!success)
        {
            return unspecified_error;
        }

        set_handle(m_handles, g_processProcessHandle, processInfo.hProcess);
        set_handle(m_handles, g_processThreadHandle, processInfo.hThread);

        return no_error;
    }

    expected<> process::wait()
    {
        const HANDLE hProcess = get_handle(m_handles, g_processProcessHandle);

        if (WaitForSingleObject(hProcess, INFINITE) != WAIT_OBJECT_0)
        {
            return unspecified_error;
        }

        return no_error;
    }

    expected<i64> process::get_exit_code()
    {
        const HANDLE hProcess = get_handle(m_handles, g_processProcessHandle);

        DWORD exitCode;

        if (!GetExitCodeProcess(hProcess, &exitCode))
        {
            return unspecified_error;
        }

        return i64{exitCode};
    }

    void process::detach()
    {
        if (const auto h = get_handle(m_handles, g_processProcessHandle))
        {
            CloseHandle(h);
            set_handle(m_handles, g_processProcessHandle, NULL);
        }

        if (const auto h = get_handle(m_handles, g_processThreadHandle))
        {
            CloseHandle(h);
            set_handle(m_handles, g_processThreadHandle, NULL);
        }
    }
}

#endif