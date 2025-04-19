#pragma once

#include <oblo/core/string/string.hpp>
#include <oblo/modules/utility/provider_service.hpp>

namespace oblo::editor
{
    struct asset_repository_descriptor
    {
        string name;
        string assetsDirectory;
        string sourcesDirectory;
    };

    using asset_repository_provider = provider_service<asset_repository_descriptor>;

    template <typename F>
    using lambda_asset_repository_provider = lambda_provider_service<asset_repository_descriptor, F>;
}