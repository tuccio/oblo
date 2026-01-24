#pragma once

#include <oblo/asset/descriptors/file_importer_descriptor.hpp>
#include <oblo/core/any.hpp>
#include <oblo/core/expected.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/uuid.hpp>

#include <any>

namespace oblo
{
    class any_asset;

    using asset_create_fn = any_asset (*)(const any& userdata);
    using load_asset_fn = expected<> (*)(any_asset& asset, cstring_view source, const any& userdata);
    using save_asset_fn = expected<> (*)(
        const any_asset& asset, cstring_view destination, cstring_view workDir, const any& userdata);

    struct native_asset_descriptor
    {
        uuid typeUuid{};
        type_id typeId;
        cstring_view fileExtension;
        asset_create_fn create{};
        load_asset_fn load{};
        save_asset_fn save{};
        create_file_importer_fn createImporter{};
        any userdata;
    };
}