#pragma once

#include <oblo/core/string/cstring_view.hpp>

#define OBLO_ASSET_PATH_PREFIX "$"
#define OBLO_ASSET_PATH(Str) OBLO_ASSET_PATH_PREFIX Str

namespace oblo
{
    constexpr cstring_view asset_path_prefix = OBLO_ASSET_PATH_PREFIX;
}
