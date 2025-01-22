#include <oblo/asset/utility/registration.hpp>

#include <oblo/asset/asset_registry.hpp>
#include <oblo/asset/descriptors/artifact_type_descriptor.hpp>
#include <oblo/asset/import/file_importers_provider.hpp>
#include <oblo/asset/import/importer.hpp>
#include <oblo/resource/descriptors/resource_type_descriptor.hpp>
#include <oblo/resource/providers/resource_types_provider.hpp>

namespace oblo
{
    void register_asset_types(asset_registry& registry, std::span<const resource_types_provider* const> providers)
    {
        deque<resource_type_descriptor> resourceTypes;

        for (auto* const provider : providers)
        {
            resourceTypes.clear();
            provider->fetch_resource_types(resourceTypes);

            for (const auto& resourceType : resourceTypes)
            {
                registry.register_type(artifact_type_descriptor{resourceType});
            }
        }
    }

    void register_file_importers(asset_registry& registry, std::span<const file_importers_provider* const> providers)
    {
        dynamic_array<file_importer_desc> importers;
        importers.reserve(128);

        for (auto* const provider : providers)
        {
            importers.clear();
            provider->fetch_importers(importers);

            for (const auto& importer : importers)
            {
                registry.register_file_importer(importer);
            }
        }
    }
}