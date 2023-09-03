#pragma once

namespace oblo::resource
{
    class resource_registry;
}

namespace oblo::asset
{
    class asset_registry;
}

namespace oblo::scene
{
    void register_asset_types(asset::asset_registry& registry);
    void register_resource_types(resource::resource_registry& registry);
}