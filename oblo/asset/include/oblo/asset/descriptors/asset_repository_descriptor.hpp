#pragma once

#include <oblo/core/flags.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/string/hashed_string_view.hpp>

namespace oblo
{
    enum class asset_repository_flags : u8
    {
        /// @brief Will not add the path to the originally imported file to the import info.
        /// This will make it impossible to re-import from the original file, but it might make sense when sharing the
        /// repository with others.
        omit_import_source_path,
        /// @brief Blocks asset discovery until importing is done.
        wait_until_processed,
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