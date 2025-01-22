#ifdef WIN32

    #include <oblo/core/array_size.hpp>
    #include <oblo/core/filesystem/directory_watcher.hpp>
    #include <oblo/core/filesystem/filesystem.hpp>
    #include <oblo/core/string/string_builder.hpp>

    #include <filesystem>

    #include <Windows.h>

    #include <sys/stat.h>

namespace oblo::filesystem
{
    namespace
    {
        using posix_stat = struct ::_stat;

        expected<posix_stat> file_stat(const std::filesystem::path& nativePath)
        {
            posix_stat st;
            const int res = _wstat(nativePath.native().c_str(), &st);

            if (res != 0)
            {
                return unspecified_error;
            }

            return st;
        }

        struct modification_tracker
        {
            decltype(posix_stat::st_mtime) modificationTime;
            decltype(posix_stat::st_size) size;
            u64 nameHash;

            bool operator==(const modification_tracker&) const = default;

            static modification_tracker make(const std::filesystem::path& p)
            {
                const auto st = file_stat(p).value_or({});

                const auto& str = p.native();
                const u64 h = hash_xxh64(str.c_str(), str.size());

                return modification_tracker{
                    .modificationTime = st.st_mtime,
                    .size = st.st_size,
                    .nameHash = h,
                };
            }
        };

        u64 do_hash(u64* out, const std::filesystem::path& p)
        {
            const auto& str = p.native();
            const u64 r = hash_xxh64(str.c_str(), str.size());
            *out = r;
            return r;
        }
    }

    struct directory_watcher::impl
    {
        ~impl()
        {
            if (hDirectory)
            {
                CloseHandle(hDirectory);
            }

            if (hEvent)
            {
                CloseHandle(hEvent);
            }
        }

        bool read_changes()
        {
            constexpr DWORD notifyFilter{
                FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
            };

            return ReadDirectoryChangesW(hDirectory,
                       buffer,
                       sizeof(buffer),
                       isRecursive,
                       notifyFilter,
                       nullptr,
                       &overlapped,
                       nullptr) != 0;
        }

        bool isRecursive;

        HANDLE hDirectory{};
        HANDLE hEvent{};

        OVERLAPPED overlapped{};

        string_builder path;
        std::filesystem::path nativePath;
        std::filesystem::path nativePathBuffer;

        bool isExpectingRenameNewName{};

        alignas(void*) u8 buffer[1024];
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

        const string_view directory = initializer.path;

        wchar_t path[MAX_PATH + 1];

        const int pathLen = MultiByteToWideChar(CP_UTF8, 0, directory.data(), directory.size32(), path, MAX_PATH);

        if (pathLen < 0)
        {
            return unspecified_error;
        }

        path[pathLen] = wchar_t{};

        std::error_code ec;
        m_impl->nativePath = std::filesystem::absolute(path, ec);

        if (ec)
        {
            return unspecified_error;
        }

        m_impl->path.append(m_impl->nativePath.c_str());

        m_impl->hDirectory = CreateFileW(m_impl->nativePath.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
            nullptr);

        if (!m_impl->hDirectory)
        {
            return unspecified_error;
        }

        m_impl->hEvent = CreateEvent(nullptr, FALSE, FALSE, NULL);

        if (!m_impl->hEvent)
        {
            return unspecified_error;
        }

        m_impl->overlapped = {
            .hEvent = m_impl->hEvent,
        };

        if (!m_impl->read_changes())
        {
            return unspecified_error;
        }

        return no_error;
    }

    void directory_watcher::shutdown()
    {
        m_impl.reset();
    }

    expected<> directory_watcher::process(callback_fn callback) const
    {
        if (!m_impl)
        {
            return unspecified_error;
        }

        modification_tracker lastModification{};

        string_builder builder;
        string_builder auxBuilder;

        while (true)
        {
            constexpr DWORD timeoutMs = 0;
            const auto waitResult = WaitForSingleObject(m_impl->hEvent, timeoutMs);

            if (waitResult == WAIT_OBJECT_0)
            {
                DWORD bytes = 0;

                const BOOL ovelappedResult =
                    GetOverlappedResult(m_impl->hDirectory, &m_impl->overlapped, &bytes, FALSE);

                if (ovelappedResult == 0)
                {
                    return unspecified_error;
                }
                else
                {
                    DWORD offset = 0;

                    do
                    {
                        const auto* const fi = new (m_impl->buffer + offset) FILE_NOTIFY_INFORMATION;

                        directory_watcher_event_kind eventKind{};
                        bool skip{};

                        switch (fi->Action)
                        {
                        case FILE_ACTION_ADDED:
                            eventKind = directory_watcher_event_kind::created;
                            break;

                        case FILE_ACTION_REMOVED:
                            eventKind = directory_watcher_event_kind::removed;
                            break;

                        case FILE_ACTION_RENAMED_OLD_NAME: {
                            auto* const fileNameBegin = fi->FileName;
                            auto* const fileNameEnd = fi->FileName + fi->FileNameLength / sizeof(wchar_t);
                            m_impl->nativePathBuffer = std::wstring_view{fileNameBegin, fileNameEnd};

                            skip = true;
                        }
                        break;

                        case FILE_ACTION_RENAMED_NEW_NAME:
                            OBLO_ASSERT(m_impl->isExpectingRenameNewName);
                            eventKind = directory_watcher_event_kind::renamed;
                            skip = !m_impl->isExpectingRenameNewName;
                            break;

                        case FILE_ACTION_MODIFIED:
                            eventKind = directory_watcher_event_kind::modified;
                            break;

                        default:
                            skip = true;
                            break;
                        }

                        m_impl->isExpectingRenameNewName = fi->Action == FILE_ACTION_RENAMED_OLD_NAME;

                        if (!skip)
                        {
                            builder = m_impl->path;

                            auto* const fileNameBegin = fi->FileName;
                            auto* const fileNameEnd = fi->FileName + fi->FileNameLength / sizeof(wchar_t);

                            builder.append_path_separator().append(fileNameBegin, fileNameEnd);

                            directory_watcher_event evt{
                                .path = builder,
                                .eventKind = eventKind,
                            };

                            bool sendEvent = true;

                            // Try to reduce the spam of modified events a little bit
                            if (eventKind == directory_watcher_event_kind::modified)
                            {
                                m_impl->nativePathBuffer = m_impl->nativePath;
                                m_impl->nativePathBuffer.append(std::wstring_view{fileNameBegin, fileNameEnd});

                                const auto newModification = modification_tracker::make(m_impl->nativePathBuffer);

                                sendEvent = newModification != lastModification;

                                if (sendEvent)
                                {
                                    lastModification = newModification;
                                }
                            }
                            else if (eventKind == directory_watcher_event_kind::renamed)
                            {
                                auxBuilder = m_impl->path;

                                auxBuilder.append_path_separator().append(m_impl->nativePathBuffer.native().c_str(),
                                    m_impl->nativePathBuffer.c_str() + m_impl->nativePathBuffer.native().size());

                                evt.previousName = auxBuilder;
                            }

                            if (sendEvent)
                            {
                                callback(evt);
                            }
                        }

                        offset = fi->NextEntryOffset;
                    } while (offset > 0);
                }

                if (!m_impl->read_changes())
                {
                    return unspecified_error;
                }
            }
            else if (waitResult == WAIT_TIMEOUT)
            {
                break;
            }
            else
            {
                return unspecified_error;
            }
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