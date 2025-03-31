#pragma once

#include <oblo/asset/any_asset.hpp>
#include <oblo/core/flags.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/unique_ptr.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/editor/services/asset_editor.hpp>
#include <oblo/editor/window_manager.hpp>
#include <oblo/modules/utility/provider_service.hpp>

namespace oblo::editor
{
    enum class asset_editor_flags : u8
    {
        unique_type,
        enum_max,
    };

    using asset_create_fn = any_asset (*)();
    using asset_editor_create_fn = unique_ptr<asset_editor> (*)();

    struct asset_editor_descriptor
    {
        uuid assetType{};
        cstring_view category;
        cstring_view name;
        asset_create_fn create{};
        asset_editor_create_fn createEditor{};
        flags<asset_editor_flags> flags{};
    };

    using asset_editor_provider = provider_service<asset_editor_descriptor>;
}