#pragma once

namespace oblo::resource
{
    class registry;
}

namespace oblo::asset
{
    class registry;
}

namespace oblo::scene
{
    SCENE_API void register_asset_types(asset::registry& registry);
    SCENE_API void unregister_asset_types(asset::registry& registry);
    
    SCENE_API void register_resource_types(resource::registry& registry);
    SCENE_API void unregister_resource_types(resource::registry& registry);
}