#pragma once

#include <oblo/asset/descriptors/native_asset_descriptor.hpp>
#include <oblo/modules/utility/provider_service.hpp>

namespace oblo
{
    using native_asset_provider = provider_service<native_asset_descriptor>;
}