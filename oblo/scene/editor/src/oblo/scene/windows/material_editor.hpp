#pragma once

#include <oblo/asset/any_asset.hpp>
#include <oblo/core/uuid.hpp>

namespace oblo
{
    class asset_registry;
}

namespace oblo::editor
{
    struct window_update_context;

    class material_editor final
    {
    public:
        material_editor(uuid assetId);

        bool init(const window_update_context& ctx);
        bool update(const window_update_context& ctx);

        void set_asset(const uuid& assetId);

    private:
        asset_registry* m_assetRegistry{};
        uuid m_assetId{};
        any_asset m_asset;
    };
}