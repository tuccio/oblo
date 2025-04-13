#pragma once

#include <oblo/core/string/string_builder.hpp>

#include <cstdio>

namespace oblo
{
    template <typename... Args>
    void print(std::format_string<Args...> fmt, Args&&... args)
    {
        string_builder b;
        b.format(fmt, std::forward<Args>(args)...);
        std::fputs(b.c_str(), stdout);
    }

    template <typename... Args>
    void print_line(std::format_string<Args...> fmt, Args&&... args)
    {
        string_builder b;
        b.format(fmt, std::forward<Args>(args)...);
        b.append('\n');
        std::fputs(b.c_str(), stdout);
    }

    template <typename... Args>
    void print(std::FILE* f, std::format_string<Args...> fmt, Args&&... args)
    {
        string_builder b;
        b.format(fmt, std::forward<Args>(args)...);
        std::fputs(b.c_str(), f);
    }

    template <typename... Args>
    void print_line(std::FILE* f, std::format_string<Args...> fmt, Args&&... args)
    {
        string_builder b;
        b.format(fmt, std::forward<Args>(args)...);
        b.append('\n');
        std::fputs(b.c_str(), f);
    }
}