#include <oblo/core/platform/core.hpp>

#include <oblo/core/filesystem/filesystem.hpp>
#include <oblo/core/iterator/token_range.hpp>
#include <oblo/core/string/string_builder.hpp>

namespace oblo::platform
{
    void split_paths_environment_variable(dynamic_array<string_view>& out, const string_view value)
    {
        static_assert(is_windows() || is_unix_like());

        constexpr string_view pathSeparator = is_windows() ? ";" : ":";

        for (const string_view p : token_range{value, pathSeparator})
        {
            out.emplace_back(p);
        }
    }
}