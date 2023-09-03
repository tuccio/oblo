#pragma once

namespace oblo::asset
{
    class registry;
}

namespace oblo::asset::importers
{
    void register_gltf_importer(registry& registry);
}