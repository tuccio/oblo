#pragma once

namespace oblo
{
    class resource_registry;
}

namespace oblo
{
    class asset_registry;
}

namespace oblo
{
    SCENE_API void register_asset_types(asset_registry& registry);
    SCENE_API void unregister_asset_types(asset_registry& registry);

    SCENE_API void register_resource_types(resource_registry& registry);
    SCENE_API void unregister_resource_types(resource_registry& registry);
}