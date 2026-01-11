#include <oblo/script/compiler/cpp_compiler.hpp>

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/filesystem/filesystem.hpp>
#include <oblo/core/platform/core.hpp>
#include <oblo/core/string/string_builder.hpp>

namespace oblo
{
    expected<cpp_compiler> cpp_compiler::find(cpp_compiler::kind_flags which)
    {
        dynamic_array<string_view> searchPaths;
        searchPaths.reserve(128);

        [[maybe_unused]] string_builder winPaths;

        if constexpr (platform::is_windows() || platform::is_unix_like())
        {
            if (platform::read_environment_variable(winPaths, "PATH"))
            {
                platform::split_paths_environment_variable(searchPaths, winPaths.view());
            }
        }

        constexpr string_view clang = platform::is_windows() ? "clang++.exe" : "clang++";

        string_builder pathToExe;

        if (which.contains(kind::clang) && filesystem::search_file_in_paths(pathToExe, clang, searchPaths))
        {
            return cpp_compiler(kind::clang, pathToExe.view());
        }

        if (platform::is_unix_like())
        {
            if (which.contains(kind::gcc) && filesystem::search_file_in_paths(pathToExe, "g++", searchPaths))
            {
                return cpp_compiler(kind::gcc, pathToExe.view());
            }
        }

        return unspecified_error;
    }

    cpp_compiler::kind cpp_compiler::get_kind() const
    {
        return m_kind;
    }

    cstring_view cpp_compiler::get_path() const
    {
        return m_path;
    }

    expected<> cpp_compiler::make_shared_library_command_arguments(
        dynamic_array<string>& args, string_view src, string_view dst, const options& opts) const
    {
        string_builder buf;

        switch (m_kind)
        {
        case kind::clang:
            [[fallthrough]];
        case kind::gcc:

            args.emplace_back("-shared");

            if constexpr (platform::is_unix_like())
            {
                args.emplace_back("-fPIC");
            }

            switch (opts.optimizations)
            {
            case options::optimization_level::none:
                args.emplace_back("-O0");
                break;

            case options::optimization_level::low:
                args.emplace_back("-O1");
                break;

            case options::optimization_level::high:
                args.emplace_back("-O2");
                break;

            case options::optimization_level::highest:
                args.emplace_back("-O3");
                break;
            }

            args.emplace_back(src);

            args.emplace_back("-o");
            args.emplace_back(dst);

            break;

        case kind::msvc:
            args.emplace_back("/LD");
            args.emplace_back(src);
            args.emplace_back(buf.clear().format("/Fe:{}", dst).view());

            switch (opts.optimizations)
            {
            case options::optimization_level::none:
                args.emplace_back("/O0");
                break;

            case options::optimization_level::low:
                args.emplace_back("/O1");
                break;

            case options::optimization_level::high:
                [[fallthrough]];
            case options::optimization_level::highest:
                args.emplace_back("/O2");
                break;
            }
            break;
        }

        return no_error;
    }
}