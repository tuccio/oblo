#pragma once

#include <oblo/core/string/cstring_view.hpp>

namespace oblo
{
    constexpr cstring_view asset_path_prefix = "$";
}

#define OBLO_ASSET_PATH(Str) "$" Str