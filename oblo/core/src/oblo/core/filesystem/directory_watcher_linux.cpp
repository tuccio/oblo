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
        int watchFd{-1};
        bool isRecursive{};

        string_builder path;

        // Maps the cookie for a MOVE_FROM event, to keep track of the previous name
        std::unordered_map<u32, pending_rename_event> pendingRenameEvents;

        alignas(void*) u8 buffer[4096];
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

        constexpr uint32_t mask = IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO;

        m_impl->watchFd = inotify_add_watch(m_impl->inotifyFd, m_impl->path.c_str(), mask);

        if (m_impl->watchFd < 0)
        {
            shutdown();
            return unspecified_error;
        }

        return no_error;
    }

    void directory_watcher::shutdown()
    {
        if (m_impl)
        {
            if (m_impl->watchFd >= 0)
            {
                inotify_rm_watch(m_impl->inotifyFd, m_impl->watchFd);
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

        decltype(m_impl->pendingRenameEvents)::node_type pendingRename;

        for (ssize_t offset = 0; offset < bytesRead;)
        {
            const auto* const evt = reinterpret_cast<const inotify_event*>(m_impl->buffer + offset);

            directory_watcher_event_kind kind{};
            bool sendEvent{true};

            if (evt->mask & IN_CREATE)
            {
                kind = directory_watcher_event_kind::created;
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

                if (evt->cookie > 0 && evt->len > 0) [[likely]]
                {
                    auto& renameEvt = m_impl->pendingRenameEvents[evt->cookie];
                    renameEvt.oldName.append(evt->name);
                }

                sendEvent = false;
            }
            else if (evt->mask & IN_MOVED_TO)
            {
                const auto it = m_impl->pendingRenameEvents.find(evt->cookie);

                if (it != m_impl->pendingRenameEvents.end())
                {
                    kind = directory_watcher_event_kind::renamed;
                    pendingRename = m_impl->pendingRenameEvents.extract(it);
                }
                else
                {
                    sendEvent = false;
                }
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
                    e.previousName = pendingRename.mapped().oldName;
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
