#include <oblo/dotnet/assets/dotnet_script_asset.hpp>

namespace oblo
{
    string_builder& dotnet_script_asset::code()
    {
        return m_code;
    }

    const string_builder& dotnet_script_asset::code() const
    {
        return m_code;
    }
}