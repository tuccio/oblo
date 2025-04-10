#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/invoke/function_ref.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/string/string_view.hpp>

namespace oblo::dotnet_utility
{
    DOTNET_ASSET_API expected<> generate_csproj(cstring_view path);
    DOTNET_ASSET_API expected<> find_cs_files(cstring_view directory, function_ref<void(cstring_view)> cb);
}