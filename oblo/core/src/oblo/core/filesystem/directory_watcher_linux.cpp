#ifdef __linux__

    #include <oblo/core/array_size.hpp>
    #include <oblo/core/filesystem/directory_watcher.hpp>
    #include <oblo/core/filesystem/filesystem.hpp>
    #include <oblo/core/string/hashed_string_view.hpp>
    #include <oblo/core/string/string_builder.hpp>

    #include <sys/inotify.h>
    #include <sys/stat.h>
    #include <unistd.h>

    #include <cerrno>
    #include <cstring>
    #include <unordered_map>

namespace oblo::filesystem
{
    struct directory_watcher::impl
    {
        int inotifyFd{-1};
        bool isRecursive{};

        string_builder path;

        std::unordered_map<hashed_string_view, int, hash<hashed_string_view>> pathToWd;
        std::unordered_map<int, string_builder> wdToPath;

        alignas(void*) u8 buffer[4096];

        expected<> build_full_path(string_builder& out, int wd, cstring_view path)
        {
            const auto it = wdToPath.find(wd);

            if (it == wdToPath.end())
            {
                return unspecified_error;
            }

            out = it->second;
            out.append_path(path);
            return no_error;
        }

        expected<> add_watch(cstring_view directoryPath)
        {
            if (!add_watch_impl(directoryPath))
            {
                return unspecified_error;
            }

            if (isRecursive)
            {
                string_builder buf;

                const auto r = filesystem::walk(directoryPath,
                    [this, &buf](const filesystem::walk_entry& e)
                    {
                        if (e.is_directory())
                        {
                            e.append_full_path(buf.clear());
                            add_watch_impl(buf);
                        }

                        return walk_result::walk;
                    });

                return r;
            }

            return no_error;
        }

        expected<> remove_watch_subtree(cstring_view directoryPath)
        {
            OBLO_ASSERT(isRecursive);

            remove_watch_from_full_path(directoryPath.as<hashed_string_view>());

            string_builder buf;

            const auto r = filesystem::walk(directoryPath,
                [this, &buf](const filesystem::walk_entry& e)
                {
                    if (e.is_directory())
                    {
                        e.append_full_path(buf.clear());

                        remove_watch_from_full_path(buf.as<hashed_string_view>());
                    }

                    return walk_result::walk;
                });

            return r;
        }

        void remove_watch_from_wd(int wd)
        {
            const auto it = wdToPath.find(wd);

            if (it != wdToPath.end())
            {
                pathToWd.erase(it->second.as<hashed_string_view>());
                wdToPath.erase(it);
            }
        }

        void remove_watch_from_full_path(hashed_string_view fullPath)
        {
            const auto it = pathToWd.find(fullPath);

            if (it != pathToWd.end())
            {
                const int wd = it->second;
                pathToWd.erase(it);
                wdToPath.erase(wd);
            }
        }

    private:
        bool add_watch_impl(const cstring_view& directoryPath)
        {
            static constexpr uint32_t mask =
                IN_CREATE | IN_DELETE | IN_CLOSE_WRITE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO | IN_IGNORED;

            const int wd = inotify_add_watch(inotifyFd, directoryPath.c_str(), mask);

            if (wd < 0)
            {
                return false;
            }

            const auto [it, inserted] = wdToPath.emplace(wd, string_builder{});

            if (!inserted)
            {
                pathToWd.erase(it->second.as<hashed_string_view>());
            }

            it->second = directoryPath;
            pathToWd[it->second.as<hashed_string_view>()] = wd;
            return true;
        }
    };

    directory_watcher::directory_watcher() = default;
    directory_watcher::directory_watcher(directory_watcher&&) noexcept = default;
    directory_watcher& directory_watcher::operator=(directory_watcher&&) noexcept = default;
    directory_watcher::~directory_watcher() = default;

    expected<> directory_watcher::init(const directory_watcher_initializer& initializer)
    {
        shutdown();

        m_impl = allocate_unique<impl>();
        m_impl->isRecursive = initializer.isRecursive;

        string_builder builder;

        if (!absolute(initializer.path, builder))
        {
            return unspecified_error;
        }

        m_impl->path = builder.c_str();

        m_impl->inotifyFd = inotify_init1(IN_NONBLOCK);

        if (m_impl->inotifyFd < 0)
        {
            return unspecified_error;
        }

        const auto r = m_impl->add_watch(m_impl->path.c_str());

        if (!r)
        {
            shutdown();
        }

        return r;
    }

    void directory_watcher::shutdown()
    {
        if (m_impl)
        {
            for (const auto& [fd, path] : m_impl->wdToPath)
            {
                inotify_rm_watch(m_impl->inotifyFd, fd);
            }

            if (m_impl->inotifyFd >= 0)
            {
                close(m_impl->inotifyFd);
            }

            m_impl.reset();
        }
    }

    expected<> directory_watcher::process(callback_fn callback) const
    {
        if (!m_impl)
        {
            return unspecified_error;
        }

        const ssize_t bytesRead = read(m_impl->inotifyFd, m_impl->buffer, sizeof(m_impl->buffer));

        if (bytesRead <= 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return no_error;
            }
            return unspecified_error;
        }

        string_builder fullPath;

        // Maps the cookie for a MOVE_FROM event, to keep track of the previous name
        struct pending_rename_info
        {
            string_builder oldName;
            int cookie{};
            bool isDir{};
        };

        pending_rename_info pendingRename;

        const auto try_flush_pending_moved_from = [&pendingRename, &callback, impl = m_impl.get()]
        {
            if (pendingRename.cookie == 0)
            {
                return;
            }

            const directory_watcher_event e{
                .path = pendingRename.oldName,
                .eventKind = directory_watcher_event_kind::removed,
            };

            callback(e);

            const bool isWatchSubdir = pendingRename.isDir && impl->isRecursive;

            if (isWatchSubdir)
            {
                impl->remove_watch_subtree(pendingRename.oldName).assert_value();
            }
        };

        for (ssize_t offset = 0; offset < bytesRead;)
        {
            auto* const evt = reinterpret_cast<const inotify_event*>(m_impl->buffer + offset);

            directory_watcher_event_kind kind{};
            bool sendEvent{true};

            if (evt->mask & IN_CREATE)
            {
                kind = directory_watcher_event_kind::added;

                if (m_impl->isRecursive && evt->mask & IN_ISDIR)
                {
                    if (m_impl->build_full_path(fullPath, evt->wd, evt->name))
                    {
                        m_impl->add_watch(fullPath.as<cstring_view>()).assert_value();
                    }
                }
            }
            else if (evt->mask & IN_DELETE)
            {
                kind = directory_watcher_event_kind::removed;
            }
            else if (evt->mask & (IN_MODIFY | IN_CLOSE_WRITE))
            {
                kind = directory_watcher_event_kind::modified;
            }
            else if (evt->mask & IN_MOVED_FROM)
            {
                OBLO_ASSERT(evt->cookie != 0);

                if (evt->cookie != 0 && evt->len > 0) [[likely]]
                {
                    try_flush_pending_moved_from();

                    pendingRename.cookie = evt->cookie;
                    pendingRename.oldName = evt->name;
                    pendingRename.isDir = (evt->mask & IN_ISDIR) != 0;
                }

                sendEvent = false;
            }
            else if (evt->mask & IN_MOVED_TO)
            {
                const bool isWatchSubdir = m_impl->isRecursive && evt->mask & IN_ISDIR;

                if (pendingRename.cookie == evt->cookie)
                {
                    kind = directory_watcher_event_kind::renamed;

                    if (isWatchSubdir)
                    {
                        // TODO: Adjust the paths of the whole subtree if necessary
                    }
                }
                else
                {
                    kind = directory_watcher_event_kind::added;

                    if (isWatchSubdir)
                    {
                        fullPath = m_impl->path;
                        fullPath.append_path_separator().append(evt->name);

                        m_impl->add_watch(fullPath).assert_value();
                    }
                }
            }
            else if (evt->mask & IN_IGNORED)
            {
                m_impl->remove_watch_from_wd(evt->wd);
                sendEvent = false;
            }
            else
            {
                sendEvent = false;
            }

            if (sendEvent && evt->len > 0)
            {
                fullPath = m_impl->path;
                fullPath.append_path_separator().append(evt->name);

                directory_watcher_event e{
                    .path = fullPath,
                    .eventKind = kind,
                };

                switch (kind)
                {
                case directory_watcher_event_kind::renamed: {
                    e.previousName = pendingRename.oldName;
                    break;
                }

                default:
                    break;
                }

                if (sendEvent)
                {
                    callback(e);
                }
            }

            offset += sizeof(inotify_event) + evt->len;
        }

        try_flush_pending_moved_from();

        return no_error;
    }

    cstring_view directory_watcher::get_directory() const
    {
        if (!m_impl)
        {
            return {};
        }

        return m_impl->path;
    }
}

#endif
