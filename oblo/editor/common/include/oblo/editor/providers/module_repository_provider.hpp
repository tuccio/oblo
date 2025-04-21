#pragma once

#include <oblo/asset/descriptors/asset_repository_descriptor.hpp>
#include <oblo/core/string/string.hpp>
#include <oblo/modules/utility/provider_service.hpp>

namespace oblo::editor
{
    struct module_repository_descriptor
    {
        string name;
        string assetsDirectory;
        string sourcesDirectory;
        flags<asset_repository_flags> flags{};
    };

    using module_repository_provider = provider_service<module_repository_descriptor>;

    template <typename F>
    using lambda_module_repository_provider = lambda_provider_service<module_repository_descriptor, F>;
}