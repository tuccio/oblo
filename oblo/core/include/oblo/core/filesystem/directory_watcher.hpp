#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/flags.hpp>
#include <oblo/core/invoke/function_ref.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/unique_ptr.hpp>

#include <span>

namespace oblo::filesystem
{
    struct directory_watcher_event;

    enum class directory_watcher_event_kind : u8
    {
        created,
        modified,
        removed,
        renamed,
        enum_max,
    };

    struct directory_watcher_initializer
    {
        string_view path;
        bool isRecursive = false;
    };

    class directory_watcher
    {
    public:
        struct event;

        using callback_fn = function_ref<void(const directory_watcher_event& evt)>;

    public:
        directory_watcher();
        directory_watcher(const directory_watcher&) = delete;
        directory_watcher(directory_watcher&&) noexcept;

        directory_watcher& operator=(const directory_watcher&) = delete;
        directory_watcher& operator=(directory_watcher&&) noexcept;

        ~directory_watcher();

        expected<> init(const directory_watcher_initializer& initializer);
        void shutdown();

        expected<> process(callback_fn callback) const;

        cstring_view get_directory() const;

    private:
        struct impl;
        unique_ptr<impl> m_impl;
    };

    struct directory_watcher_event
    {
        cstring_view path;
        directory_watcher_event_kind eventKind;

        /// @brief The previous name in a rename event.
        cstring_view previousName;
    };
}