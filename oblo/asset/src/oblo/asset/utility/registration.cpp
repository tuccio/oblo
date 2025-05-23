#include <oblo/asset/utility/registration.hpp>

#include <oblo/asset/asset_registry.hpp>
#include <oblo/asset/descriptors/file_importer_descriptor.hpp>
#include <oblo/asset/descriptors/native_asset_descriptor.hpp>
#include <oblo/asset/import/file_importer.hpp>
#include <oblo/asset/import/file_importers_provider.hpp>
#include <oblo/asset/providers/native_asset_provider.hpp>
#include <oblo/resource/descriptors/resource_type_descriptor.hpp>
#include <oblo/resource/providers/resource_types_provider.hpp>

namespace oblo
{
    void register_file_importers(asset_registry& registry, std::span<const file_importers_provider* const> providers)
    {
        dynamic_array<file_importer_descriptor> importers;
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

    void register_native_asset_types(asset_registry& registry, std::span<const native_asset_provider* const> providers)
    {
        deque<native_asset_descriptor> nativeAssetTypes;

        for (auto* const provider : providers)
        {
            nativeAssetTypes.clear();
            provider->fetch(nativeAssetTypes);

            for (auto& nativeAssetType : nativeAssetTypes)
            {
                registry.register_native_asset_type(std::move(nativeAssetType));
            }
        }
    }
}