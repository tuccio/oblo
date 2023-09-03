#pragma once

namespace oblo::resource
{
    class registry;
}

namespace oblo::asset
{
    class asset_registry;
}

namespace oblo::scene
{
    void register_asset_types(asset::asset_registry& registry);
    void register_resource_types(resource::registry& registry);
}