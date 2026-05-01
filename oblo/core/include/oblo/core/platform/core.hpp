#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/forward.hpp>
#include <oblo/core/types.hpp>

namespace oblo::platform
{
    bool init();
    void shutdown();

    void debug_output(const char* str);
    bool is_debugger_attached();

    void wait_for_attached_debugger();

    [[nodiscard]] bool read_environment_variable(string_builder& out, cstring_view key);
    [[nodiscard]] bool write_environment_variable(cstring_view key, cstring_view value);

    void split_paths_environment_variable(dynamic_array<string_view>& out, const string_view value);

    [[nodiscard]] bool get_main_executable_path(string_builder& out);

    expected<usize> get_ram_usage();

    consteval bool is_windows() noexcept
    {
#ifdef WIN32
        return true;
#else
        return false;
#endif
    }

    consteval bool is_linux() noexcept
    {
#ifdef __linux__
        return true;
#else
        return false;
#endif
    }

    consteval bool is_unix_like() noexcept
    {
        return is_linux();
    }

    consteval bool is_x86_64() noexcept
    {
#if defined(__x86_64__) || defined(_M_X64)
        return true;
#else
        return false;
#endif
    }

    consteval bool is_avx2() noexcept
    {
#if defined(__AVX2__)
        return true;
#else
        return false;
#endif
    }

    consteval bool is_little_endian() noexcept
    {
        constexpr u32 little{0x41424344u};
        constexpr u32 native{'ABCD'};

        return native == little;
    }

    consteval bool is_big_endian() noexcept
    {
        return !is_little_endian();
    }

    enum class endian : u8
    {
        little,
        big,
        native = is_little_endian() ? little : big,
    };
}