#ifdef __linux__

    #include <oblo/core/array_size.hpp>
    #include <oblo/core/filesystem/directory_watcher.hpp>
    #include <oblo/core/filesystem/filesystem.hpp>
    #include <oblo/core/string/string_builder.hpp>

    #include <sys/inotify.h>
    #include <sys/stat.h>
    #include <unistd.h>

    #include <cerrno>
    #include <cstring>
    #include <unordered_map>

namespace oblo::filesystem
{
    namespace
    {
        struct pending_rename_event
        {
            string_builder oldName;
        };

    }

    struct directory_watcher::impl
    {
        int inotifyFd{-1};
        bool isRecursive{};

        string_builder path;

        std::unordered_map<int, string_builder> watches;

        alignas(void*) u8 buffer[4096];

        expected<> build_full_path(string_builder& out, int wd, cstring_view path)
        {
            const auto it = watches.find(wd);

            if (it == watches.end())
            {
                return unspecified_error;
            }

            out = it->second;
            out.append_path(path);
            return no_error;
        }

        expected<> add_watch(cstring_view directoryPath)
        {
            static constexpr uint32_t mask =
                IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO | IN_IGNORED;

            {
                const int fd = inotify_add_watch(inotifyFd, directoryPath.c_str(), mask);

                if (fd < 0)
                {
                    return unspecified_error;
                }

                watches[fd] = directoryPath;
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

                            const int fd = inotify_add_watch(inotifyFd, buf.c_str(), mask);

                            if (fd >= 0)
                            {
                                watches[fd] = buf.c_str();
                            }
                        }

                        return walk_result::walk;
                    });

                return r;
            }

            return no_error;
        }

        void on_ignored_event(int wd)
        {
            watches.erase(wd);
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
            for (const auto& [fd, path] : m_impl->watches)
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
        string_builder pendingRenameOldName;
        int pendingRenameCookie{};

        const auto try_flush_pending_moved_from = [&pendingRenameOldName, &pendingRenameCookie, &callback]
        {
            if (pendingRenameCookie == 0)
            {
                return;
            }

            const directory_watcher_event e{
                .path = pendingRenameOldName,
                .eventKind = directory_watcher_event_kind::removed,
            };

            callback(e);
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
            else if (evt->mask & IN_MODIFY)
            {
                kind = directory_watcher_event_kind::modified;
            }
            else if (evt->mask & IN_MOVED_FROM)
            {
                OBLO_ASSERT(evt->cookie != 0);

                if (evt->cookie != 0 && evt->len > 0) [[likely]]
                {
                    try_flush_pending_moved_from();

                    pendingRenameCookie = evt->cookie;
                    pendingRenameOldName = evt->name;
                }

                sendEvent = false;
            }
            else if (evt->mask & IN_MOVED_TO)
            {
                if (pendingRenameCookie == evt->cookie)
                {
                    kind = directory_watcher_event_kind::renamed;
                }
                else
                {
                    kind = directory_watcher_event_kind::added;
                }

                if (m_impl->isRecursive && evt->mask & IN_ISDIR)
                {
                    // TODO: If a directory was moved from another directory into a watch dir, add it
                    // TODO: If a directory was moved from a watch dir outside of the watch, remove it
                }
            }
            else if (evt->mask & IN_IGNORED)
            {
                m_impl->on_ignored_event(evt->wd);
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
                    e.previousName = pendingRenameOldName;
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
