#pragma once

namespace oblo
{
    class resource_registry;
}

namespace oblo::asset
{
    class asset_registry;
}

namespace oblo::scene
{
    SCENE_API void register_asset_types(asset::asset_registry& registry);
    SCENE_API void unregister_asset_types(asset::asset_registry& registry);

    SCENE_API void register_resource_types(resource_registry& registry);
    SCENE_API void unregister_resource_types(resource_registry& registry);
}