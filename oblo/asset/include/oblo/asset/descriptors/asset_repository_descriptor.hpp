#pragma once

#include <oblo/core/flags.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/string/hashed_string_view.hpp>

namespace oblo
{
    enum class asset_repository_flags : u8
    {
        omit_import_source_path,
        enum_max,
    };

    struct asset_repository_descriptor
    {
        hashed_string_view name;
        cstring_view assetsDirectory;
        cstring_view sourcesDirectory;
        flags<asset_repository_flags> flags{};
    };
}