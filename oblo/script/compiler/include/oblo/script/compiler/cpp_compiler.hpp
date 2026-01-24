#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/flags.hpp>
#include <oblo/core/forward.hpp>
#include <oblo/core/string/string.hpp>
#include <oblo/core/types.hpp>

namespace oblo
{
    class cpp_compiler
    {
    public:
        enum class kind : u8
        {
            clang,
            gcc,
            msvc,
        };

        using kind_flags = flags<kind, 3>;

    public:
        static expected<cpp_compiler> find(kind_flags which = ~kind_flags{});

    public:
        struct options
        {
            enum class target_arch : u8
            {
                x86_64_avx2,
            };

            enum class optimization_level : u8
            {
                none,
                low,
                high,
                highest,
            };

            target_arch target{};
            optimization_level optimizations{};
            bool debugInfo{};
        };

    public:
        cpp_compiler() = default;
        cpp_compiler(kind kind, string_view path) : m_kind{kind}, m_path{path} {}
        cpp_compiler(const cpp_compiler&) = default;
        cpp_compiler(cpp_compiler&&) noexcept = default;

        ~cpp_compiler() = default;

        cpp_compiler& operator=(const cpp_compiler&) = default;
        cpp_compiler& operator=(cpp_compiler&&) noexcept = default;

        kind get_kind() const;

        cstring_view get_path() const;

        expected<> make_shared_library_command_arguments(
            dynamic_array<string>& args, string_view src, string_view dst, const options& opts) const;

    private:
        kind m_kind;
        string m_path;
    };
}