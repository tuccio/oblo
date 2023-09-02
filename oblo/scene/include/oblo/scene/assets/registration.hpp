#pragma once

namespace oblo::asset
{
    class asset_registry;
}

namespace oblo::scene
{
    void register_asset_types(asset::asset_registry& registry);
}