#include <oblo/resource/registration.hpp>

#include <oblo/resource/resource_registry.hpp>
#include <oblo/resource/resource_types_provider.hpp>
#include <oblo/resource/type_desc.hpp>

namespace oblo
{
    void register_resource_types(resource_registry& registry, std::span<const resource_types_provider* const> providers)
    {
        dynamic_array<resource_type_desc> resourceTypes;
        resourceTypes.reserve(128);

        for (auto* const provider : providers)
        {
            resourceTypes.clear();
            provider->fetch_resource_types(resourceTypes);

            for (const auto& resourceType : resourceTypes)
            {
                registry.register_type(resourceType);
            }
        }
    }
}