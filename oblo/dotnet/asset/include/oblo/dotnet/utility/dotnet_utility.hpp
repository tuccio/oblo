#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/string/string_view.hpp>

namespace oblo::dotnet_utility
{
    DOTNET_ASSET_API expected<> generate_csproj(cstring_view path);
}