#pragma once

#include <oblo/asset/any_asset.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/editor/window_manager.hpp>
#include <oblo/modules/utility/provider_service.hpp>

namespace oblo::editor
{
    using asset_create_fn = any_asset (*)();
    using asset_editor_create_fn = window_handle (*)(window_manager& window, window_handle parent, uuid assetId);

    struct asset_editor_descriptor
    {
        uuid assetType{};
        cstring_view category;
        cstring_view name;
        asset_create_fn create{};
        asset_editor_create_fn openEditorWindow{};
    };

    using asset_editor_provider = provider_service<asset_editor_descriptor>;
}