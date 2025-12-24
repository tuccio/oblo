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
    #include <filesystem>

namespace oblo::filesystem
{
    namespace
    {
        using posix_stat = struct ::stat;

        expected<posix_stat> file_stat(const std::filesystem::path& p)
        {
            posix_stat st{};
            if (::stat(p.c_str(), &st) != 0)
            {
                return unspecified_error;
            }
            return st;
        }

        struct modification_tracker
        {
            decltype(posix_stat::st_mtime) modificationTime{};
            decltype(posix_stat::st_size) size{};
            u64 nameHash{};

            bool operator==(const modification_tracker&) const = default;

            static modification_tracker make(const std::filesystem::path& p)
            {
                const auto st = file_stat(p).value_or(posix_stat{});
                const auto& str = p.native();

                return modification_tracker{
                    .modificationTime = st.st_mtime,
                    .size = st.st_size,
                    .nameHash = hash_xxh64(str.c_str(), str.size()),
                };
            }
        };
    }

    struct directory_watcher::impl
    {
        int inotifyFd{-1};
        int watchFd{-1};
        bool isRecursive{};

        string_builder path;
        std::filesystem::path nativePath;

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

        m_impl->nativePath = builder.c_str();
        m_impl->path.append(m_impl->nativePath.c_str());

        m_impl->inotifyFd = inotify_init1(IN_NONBLOCK);
        if (m_impl->inotifyFd < 0)
        {
            return unspecified_error;
        }

        constexpr uint32_t mask = IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO;

        m_impl->watchFd = inotify_add_watch(m_impl->inotifyFd, m_impl->nativePath.c_str(), mask);

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

        modification_tracker lastModification{};

        const ssize_t bytesRead = read(m_impl->inotifyFd, m_impl->buffer, sizeof(m_impl->buffer));

        if (bytesRead <= 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return no_error;
            }
            return unspecified_error;
        }

        ssize_t offset = 0;

        while (offset < bytesRead)
        {
            const auto* evt = reinterpret_cast<const inotify_event*>(m_impl->buffer + offset);

            directory_watcher_event_kind kind{};
            bool sendEvent{true};

            if (evt->mask & IN_CREATE)
                kind = directory_watcher_event_kind::created;
            else if (evt->mask & IN_DELETE)
                kind = directory_watcher_event_kind::removed;
            else if (evt->mask & IN_MODIFY)
                kind = directory_watcher_event_kind::modified;
            else if (evt->mask & (IN_MOVED_FROM | IN_MOVED_TO))
                kind = directory_watcher_event_kind::renamed;
            else
                sendEvent = false;

            if (sendEvent && evt->len > 0)
            {
                string_builder fullPath = m_impl->path;
                fullPath.append_path_separator().append(evt->name);

                directory_watcher_event e{
                    .path = fullPath,
                    .eventKind = kind,
                };

                if (kind == directory_watcher_event_kind::modified)
                {
                    const auto newModification = modification_tracker::make(fullPath.c_str());

                    sendEvent = newModification != lastModification;
                    if (sendEvent)
                    {
                        lastModification = newModification;
                    }
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
