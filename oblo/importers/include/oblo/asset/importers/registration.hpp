#pragma once

namespace oblo::asset
{
    class asset_registry;
}

namespace oblo::asset::importers
{
    void register_gltf_importer(asset_registry& registry);
}