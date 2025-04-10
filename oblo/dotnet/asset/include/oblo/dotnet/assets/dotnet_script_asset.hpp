#pragma once

#include <oblo/asset/asset_traits.hpp>
#include <oblo/core/string/string_builder.hpp>

namespace oblo
{
    class dotnet_script_asset
    {
    public:
        DOTNET_ASSET_API const string_builder& code() const;
        DOTNET_ASSET_API string_builder& code();

    private:
        string_builder m_code;
    };

    template <>
    struct asset_traits<dotnet_script_asset>
    {
        static constexpr uuid uuid = "2714ff67-db5a-4c1b-be7c-87dd14750e40"_uuid;
    };
}