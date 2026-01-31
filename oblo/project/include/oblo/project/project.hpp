#pragma once

#include <oblo/core/deque.hpp>
#include <oblo/core/expected.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/string/string.hpp>

namespace oblo
{
    struct project
    {
        string name;
        string assetsDir;
        string artifactsDir;
        string sourcesDir;
        deque<string> modules;
    };

    expected<project> project_load(cstring_view path);
    expected<> project_save(const project& project, cstring_view path);
}