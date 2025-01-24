#pragma once

#include <oblo/asset/any_asset.hpp>
#include <oblo/core/uuid.hpp>

#include <unordered_map>

namespace oblo
{
    class asset_registry;

    struct material_property_descriptor;
}

namespace oblo::editor
{
    struct window_update_context;

    class material_editor final
    {
    public:
        material_editor(uuid assetId);
        ~material_editor();

        bool init(const window_update_context& ctx);
        bool update(const window_update_context& ctx);

    private:
        asset_registry* m_assetRegistry{};
        uuid m_assetId{};
        any_asset m_asset;
        std::unordered_map<hashed_string_view, material_property_descriptor, hash<hashed_string_view>> m_propertyEditor;
    };
}