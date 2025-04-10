#pragma once

#include <oblo/asset/asset_traits.hpp>
#include <oblo/core/string/string_builder.hpp>

#include <unordered_map>

namespace oblo
{
    struct dotnet_script_asset
    {
        std::unordered_map<string, string_builder, hash<string>> scripts;
    };

    template <>
    struct asset_traits<dotnet_script_asset>
    {
        static constexpr uuid uuid = "2714ff67-db5a-4c1b-be7c-87dd14750e40"_uuid;
    };
}