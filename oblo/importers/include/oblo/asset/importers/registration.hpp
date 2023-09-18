#pragma once

namespace oblo::asset
{
    class asset_registry;
}

namespace oblo::asset::importers
{
    IMPORTERS_API void register_gltf_importer(asset_registry& registry);
    IMPORTERS_API void unregister_gltf_importer(asset_registry& registry);
}