#pragma once

#include <oblo/asset/any_asset.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/modules/utility/provider_service.hpp>

namespace oblo::editor
{
    using asset_create_fn = any_asset (*)();

    struct asset_create_descriptor
    {
        cstring_view category;
        cstring_view name;
        asset_create_fn create;
    };

    using asset_create_provider = provider_service<asset_create_descriptor>;
}