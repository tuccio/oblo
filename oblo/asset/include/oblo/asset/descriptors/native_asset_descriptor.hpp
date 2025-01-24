#pragma once

#include <oblo/asset/descriptors/file_importer_descriptor.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/uuid.hpp>

#include <any>

namespace oblo
{
    class any_asset;

    using load_asset_fn = bool (*)(any_asset& asset, cstring_view source);
    using save_asset_fn = bool (*)(const any_asset& asset, cstring_view destination, cstring_view workDir);

    struct native_asset_descriptor
    {
        uuid typeUuid;
        type_id typeId;
        cstring_view fileExtension;
        load_asset_fn load;
        save_asset_fn save;
        create_file_importer_fn createImporter;
    };
}