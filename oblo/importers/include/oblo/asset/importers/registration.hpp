#pragma once

#include <oblo/core/dynamic_array.hpp>

namespace oblo
{
    class asset_registry;
    struct file_importer_desc;
}

namespace oblo::importers
{
    IMPORTERS_API void register_gltf_importer(asset_registry& registry);
    IMPORTERS_API void unregister_gltf_importer(asset_registry& registry);

    IMPORTERS_API void register_stb_image_importer(asset_registry& registry);
    IMPORTERS_API void unregister_stb_image_importer(asset_registry& registry);

    IMPORTERS_API void fetch_importers(dynamic_array<file_importer_desc>& outResourceTypes);
}