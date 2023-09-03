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
    void register_asset_types(asset::registry& registry);
    void register_resource_types(resource::registry& registry);
}