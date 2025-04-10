#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/string/string_view.hpp>
#include <oblo/core/types.hpp>

namespace oblo
{
    class string_builder;

    template <typename>
    class function_ref;
}

namespace oblo::filesystem
{
    expected<bool> exists(cstring_view path);

    expected<bool> remove(cstring_view path);

    expected<bool> remove_all(cstring_view path);

    expected<bool> rename(cstring_view from, cstring_view to);

    expected<bool> copy_file(cstring_view source, cstring_view destination);

    expected<bool> create_directories(cstring_view path);

    expected<> create_hard_link(string_view src, string_view dst);

    expected<bool> is_directory(cstring_view path);

    expected<> absolute(cstring_view path, string_builder& out);

    bool is_relative(string_view path);

    expected<> relative(string_view path, string_view basePath, string_builder& out);

    string_view extension(string_view path);

    string_view stem(string_view path);

    cstring_view parent_path(string_view path, string_builder& out);

    cstring_view filename(cstring_view path);

    string_view filename(string_view path);

    void current_path(string_builder& out);

    enum class walk_result : u8
    {
        walk,
        stop,
    };

    class walk_entry;

    using walk_cb = function_ref<walk_result(const walk_entry& info)>;

    expected<> walk(cstring_view directory, walk_cb visit);

    class walk_entry
    {
    public:
        walk_entry() = default;
        walk_entry(const walk_entry&) = delete;
        walk_entry(walk_entry&&) noexcept = delete;

        walk_entry& operator=(const walk_entry&) = delete;
        walk_entry& operator=(walk_entry&&) noexcept = delete;

        void filename(string_builder& out) const;

        bool is_regular_file() const;
        bool is_directory() const;

    private:
        friend expected<> walk(cstring_view directory, walk_cb visit);

    private:
        struct impl;
        const impl* m_impl{};
    };
}