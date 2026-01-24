#ifdef _WIN32

    #include <oblo/core/debug.hpp>
    #include <oblo/core/filesystem/filesystem.hpp>
    #include <oblo/core/platform/core.hpp>
    #include <oblo/core/platform/file.hpp>
    #include <oblo/core/platform/platform_win32.hpp>
    #include <oblo/core/platform/process.hpp>
    #include <oblo/core/platform/shell.hpp>
    #include <oblo/core/string/utf.hpp>
    #include <oblo/core/uuid_generator.hpp>

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

    void open_file(string_view dir)
    {
        wchar_t buffer[MAX_PATH];
        win32::convert_path(dir, buffer);

        [[maybe_unused]] const auto res = ShellExecuteW(nullptr, L"open", buffer, nullptr, nullptr, SW_SHOWDEFAULT);
        OBLO_ASSERT(res != 0);
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

    bool read_environment_variable(string_builder& out, cstring_view key)
    {
        buffered_array<wchar_t, 64> wKeyBuffer;
        buffered_array<wchar_t, 1024> wValueBuffer;

        // Convert to wide chars and add null-terminator
        utf8_to_wide(key, wKeyBuffer);
        wKeyBuffer.emplace_back();

        const DWORD len = GetEnvironmentVariableW(wKeyBuffer.data(), nullptr, 0);

        if (len == 0)
        {
            return false;
        }

        wValueBuffer.resize(len);

        if (GetEnvironmentVariableW(wKeyBuffer.data(), wValueBuffer.data(), wValueBuffer.size32()) == 0)
        {
            return false;
        }

        out.assign(wValueBuffer.data(), wValueBuffer.data() + wValueBuffer.size());
        return true;
    }

    bool write_environment_variable(cstring_view key, cstring_view value)
    {
        buffered_array<wchar_t, 64> wKeyBuffer;
        buffered_array<wchar_t, 1024> wValueBuffer;

        // Convert to wide chars and add null-terminator
        utf8_to_wide(key, wKeyBuffer);
        wKeyBuffer.emplace_back();

        utf8_to_wide(value, wValueBuffer);
        wValueBuffer.emplace_back();

        return SetEnvironmentVariableW(wKeyBuffer.data(), wValueBuffer.data()) == TRUE;
    }

    process::process() = default;

    process::process(process&& other) noexcept
    {
        m_hProcess = other.m_hProcess;
        other.m_hProcess = nullptr;
    }

    process::~process()
    {
        detach();
    }

    process& process::operator=(process&& other) noexcept
    {
        detach();

        m_hProcess = other.m_hProcess;
        other.m_hProcess = nullptr;

        return *this;
    }

    expected<> process::start(const process_descriptor& desc)
    {
        detach();

        constexpr DWORD flags = CREATE_NO_WINDOW;

        STARTUPINFO startupInfo{
            .cb = sizeof(STARTUPINFO),
            .dwFlags = STARTF_USESTDHANDLES,
            .hStdInput = desc.inputStream ? desc.inputStream->get_native_handle() : nullptr,
            .hStdOutput = desc.outputStream ? desc.outputStream->get_native_handle() : nullptr,
            .hStdError = desc.errorStream ? desc.errorStream->get_native_handle() : nullptr,
        };

        PROCESS_INFORMATION processInfo{};

        string_builder cmd;

        cmd.append(desc.path);
        cmd.append(" ");
        cmd.join(desc.arguments.begin(), desc.arguments.end(), " ", "\"{}\"");

        const auto success = CreateProcessA(nullptr,
            cmd.mutable_data().data(),
            nullptr,
            nullptr,
            TRUE,
            flags,
            nullptr,
            desc.workDir.empty() ? nullptr : desc.workDir.c_str(),
            &startupInfo,
            &processInfo);

        if (!success)
        {
            return "Operation failed"_err;
        }

        m_hProcess = processInfo.hProcess;

        // We don't really have a use for this currently
        CloseHandle(processInfo.hThread);

        return no_error;
    }

    bool process::is_done()
    {
        return WaitForSingleObject(m_hProcess, 0) == WAIT_OBJECT_0;
    }

    expected<> process::wait()
    {
        if (WaitForSingleObject(m_hProcess, INFINITE) != WAIT_OBJECT_0)
        {
            return "Failed to check process status"_err;
        }

        return no_error;
    }

    expected<i64> process::get_exit_code()
    {
        DWORD exitCode;

        if (!GetExitCodeProcess(m_hProcess, &exitCode))
        {
            return "Operation failed"_err;
        }

        return i64{exitCode};
    }

    void process::detach()
    {
        if (m_hProcess)
        {
            CloseHandle(m_hProcess);
            m_hProcess = nullptr;
        }
    }

    namespace
    {
        file::error translate_file_error()
        {
            using error = file::error;

            const auto e = GetLastError();

            switch (e)
            {
            case ERROR_HANDLE_EOF:
                return error::eof;

            default:
                return error::unspecified;
            }
        }
    }

    expected<> file::create_pipe(file& readPipe, file& writePipe, u32 bufferSizeHint)
    {
        readPipe.close();
        writePipe.close();

        SECURITY_ATTRIBUTES securityAttributes{
            .nLength = sizeof(SECURITY_ATTRIBUTES),
            .bInheritHandle = TRUE,
        };

        if (!CreatePipe(&readPipe.m_handle, &writePipe.m_handle, &securityAttributes, bufferSizeHint))
        {
            return "Failed to spawn process"_err;
        }

        return no_error;
    }

    file::file() noexcept = default;

    file::file(file&& other) noexcept : m_handle{other.m_handle}
    {
        other.m_handle = nullptr;
    }

    file::~file()
    {
        close();
    }

    file& file::operator=(file&& other) noexcept
    {
        close();
        m_handle = other.m_handle;
        other.m_handle = nullptr;
        return *this;
    }

    expected<u32, file::error> file::read(void* dst, u32 size) const noexcept
    {
        DWORD actuallyRead{};

        if (!ReadFile(m_handle, dst, size, &actuallyRead, nullptr))
        {
            return translate_file_error();
        }

        return actuallyRead;
    }

    expected<u32, file::error> file::write(const void* src, u32 size) const noexcept
    {
        DWORD actuallyRead{};

        if (!WriteFile(m_handle, src, size, &actuallyRead, nullptr))
        {
            return translate_file_error();
        }

        return actuallyRead;
    }

    bool file::is_open() const noexcept
    {
        return m_handle != nullptr;
    }

    void file::close() noexcept
    {
        if (!m_handle)
        {
            return;
        }

        [[maybe_unused]] const auto handleClosed = CloseHandle(m_handle);
        OBLO_ASSERT(handleClosed);

        m_handle = {};
    }

    file::operator bool() const noexcept
    {
        return m_handle != nullptr;
    }

    file::native_handle file::get_native_handle() const noexcept
    {
        return m_handle;
    }
}

namespace oblo
{
    uuid uuid_system_generator::generate() const
    {
        GUID guid;

        if (CoCreateGuid(&guid) != RPC_S_OK)
        {
            return {};
        }

        uuid uuid;

        uuid.data[0] = u8(guid.Data1 >> 24);
        uuid.data[1] = u8(guid.Data1 >> 16);
        uuid.data[2] = u8(guid.Data1 >> 8);
        uuid.data[3] = u8(guid.Data1 >> 0);

        uuid.data[4] = u8(guid.Data2 >> 8);
        uuid.data[5] = u8(guid.Data2 >> 0);

        uuid.data[6] = u8(guid.Data3 >> 8);
        uuid.data[7] = u8(guid.Data3 >> 0);

        uuid.data[8] = guid.Data4[0];
        uuid.data[9] = guid.Data4[1];
        uuid.data[10] = guid.Data4[2];
        uuid.data[11] = guid.Data4[3];
        uuid.data[12] = guid.Data4[4];
        uuid.data[13] = guid.Data4[5];
        uuid.data[14] = guid.Data4[6];
        uuid.data[15] = guid.Data4[7];

        return uuid;
    }
}

#endif