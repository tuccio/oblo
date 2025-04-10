#ifdef WIN32

    #include <oblo/core/filesystem/filesystem.hpp>
    #include <oblo/core/invoke/function_ref.hpp>
    #include <oblo/core/platform/platform_win32.hpp>
    #include <oblo/core/string/string_builder.hpp>

    #include <Windows.h>

namespace oblo::filesystem
{
    struct walk_entry::impl
    {
        WIN32_FIND_DATAW ffd{};
    };

    expected<> walk(cstring_view directory, walk_cb visit)
    {
        walk_entry::impl impl;

        wchar_t wDir[MAX_PATH];
        auto outIt = win32::convert_path(directory, wDir);

        constexpr wchar_t suffix[] = L"\\*";

        if (outIt - wDir < sizeof(suffix))
        {
            // Not enough space in the buffer
            return unspecified_error;
        }

        std::memcpy(outIt, suffix, sizeof(suffix));

        HANDLE hFind = FindFirstFileW(wDir, &impl.ffd);

        if (hFind == INVALID_HANDLE_VALUE)
        {
            return unspecified_error;
        }

        walk_entry entry;
        entry.m_impl = &impl;

        do
        {
            const auto r = visit(entry);

            if (r == walk_result::stop)
            {
                break;
            }
        } while (FindNextFileW(hFind, &impl.ffd) != 0);

        const auto dwError = GetLastError();

        if (dwError != ERROR_NO_MORE_FILES)
        {
            return unspecified_error;
        }

        FindClose(hFind);

        return no_error;
    }

    void walk_entry::filename(string_builder& out) const
    {
        out.append(m_impl->ffd.cFileName);
    }

    bool walk_entry::is_regular_file() const
    {
        OBLO_ASSERT(m_impl);
        return (m_impl->ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    bool walk_entry::is_directory() const
    {
        OBLO_ASSERT(m_impl);
        return (m_impl->ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }
}

#endif