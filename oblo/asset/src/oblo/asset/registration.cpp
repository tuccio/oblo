#include <oblo/asset/registration.hpp>

#include <oblo/asset/asset_registry.hpp>
#include <oblo/asset/asset_type_desc.hpp>
#include <oblo/asset/file_importers_provider.hpp>
#include <oblo/asset/importer.hpp>
#include <oblo/resource/resource_types_provider.hpp>

namespace oblo
{
    void register_asset_types(asset_registry& registry, std::span<const resource_types_provider* const> providers)
    {
        dynamic_array<resource_type_desc> resourceTypes;
        resourceTypes.reserve(128);

        for (auto* const provider : providers)
        {
            resourceTypes.clear();
            provider->fetch_resource_types(resourceTypes);

            for (const auto& resourceType : resourceTypes)
            {
                registry.register_type(asset_type_desc{resourceType});
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