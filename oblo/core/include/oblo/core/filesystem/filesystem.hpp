#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/string/string_view.hpp>
#include <oblo/core/types.hpp>

namespace oblo
{
    class string_builder;
}

namespace oblo::filesystem
{
    expected<bool> exists(cstring_view path);

    expected<bool> remove(cstring_view path);

    expected<bool> remove_all(cstring_view path);

    expected<bool> rename(cstring_view from, cstring_view to);

    expected<bool> copy_file(cstring_view source, cstring_view destination);

    expected<bool> create_directories(cstring_view path);

    expected<bool> is_directory(cstring_view path);

    expected<> absolute(cstring_view path, string_builder& out);

    bool is_relative(string_view path);

    expected<> relative(string_view path, string_view basePath, string_builder& out);

    string_view extension(string_view path);

    string_view stem(string_view path);

    string_view parent_path(string_view path);

    cstring_view filename(cstring_view path);

    string_view filename(string_view path);
}