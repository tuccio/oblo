#pragma once

#include <oblo/core/type_id.hpp>

#include <filesystem>

namespace oblo::asset
{
    using create_asset_fn = void* (*) ();
    using destroy_asset_fn = void (*)(void*);
    using load_asset_fn = void (*)(void* asset, const std::filesystem::path& source);
    using save_asset_fn = void (*)(const void* asset, const std::filesystem::path& destination);

    struct asset_type_desc
    {
        type_id type;
        create_asset_fn create;
        destroy_asset_fn destroy;
        load_asset_fn load;
        save_asset_fn save;
    };
}