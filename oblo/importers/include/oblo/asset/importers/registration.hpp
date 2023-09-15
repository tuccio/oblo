#pragma once

namespace oblo::asset
{
    class registry;
}

namespace oblo::asset::importers
{
    IMPORTERS_API void register_gltf_importer(registry& registry);
    IMPORTERS_API void unregister_gltf_importer(registry& registry);
}