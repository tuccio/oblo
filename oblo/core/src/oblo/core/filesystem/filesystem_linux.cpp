#ifdef __linux__

    #include <oblo/core/filesystem/filesystem.hpp>
    #include <oblo/core/invoke/function_ref.hpp>
    #include <oblo/core/string/string_builder.hpp>

    #include <cstring>
    #include <dirent.h>
    #include <sys/stat.h>
    #include <unistd.h>

namespace oblo::filesystem
{
    struct walk_entry::impl
    {
        dirent* entry{};
        struct stat st{};
        cstring_view parent{};
    };

    expected<> walk(cstring_view directory, walk_cb visit)
    {
        DIR* const dir = opendir(directory.c_str());

        if (!dir)
        {
            return unspecified_error;
        }

        walk_entry::impl impl{};
        impl.parent = directory;

        walk_entry entry;
        entry.m_impl = &impl;

        while ((impl.entry = readdir(dir)) != nullptr)
        {
            const char* name = impl.entry->d_name;

            // Skip "." and ".."
            if (std::strcmp(name, ".") == 0 || std::strcmp(name, "..") == 0)
            {
                continue;
            }

            // Build full path for stat()
            char path[PATH_MAX];
            std::snprintf(path, sizeof(path), "%s/%s", impl.parent.c_str(), name);

            if (stat(path, &impl.st) != 0)
            {
                // Ignore entries we can't stat
                continue;
            }

            const auto r = visit(entry);

            if (r == walk_result::stop)
            {
                break;
            }
        }

        closedir(dir);
        return no_error;
    }

    void walk_entry::append_filename(string_builder& out) const
    {
        OBLO_ASSERT(m_impl);
        out.append(m_impl->entry->d_name);
    }

    void walk_entry::append_full_path(string_builder& out) const
    {
        OBLO_ASSERT(m_impl);

        out.append(m_impl->parent);
        out.append_path(m_impl->entry->d_name);
    }

    bool walk_entry::is_regular_file() const
    {
        OBLO_ASSERT(m_impl);
        return S_ISREG(m_impl->st.st_mode);
    }

    bool walk_entry::is_directory() const
    {
        OBLO_ASSERT(m_impl);
        return S_ISDIR(m_impl->st.st_mode);
    }
}

#endif
