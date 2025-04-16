#include <oblo/resource/utility/registration.hpp>

#include <oblo/resource/descriptors/resource_type_descriptor.hpp>
#include <oblo/resource/providers/resource_types_provider.hpp>
#include <oblo/resource/resource_registry.hpp>

namespace oblo
{
    void register_resource_types(resource_registry& registry, std::span<const resource_types_provider* const> providers)
    {
        deque<resource_type_descriptor> resourceTypes;

        for (auto* const provider : providers)
        {
            resourceTypes.clear();
            provider->fetch_resource_types(resourceTypes);

            for (auto& resourceType : resourceTypes)
            {
                registry.register_type(std::move(resourceType));
            }
        }
    }
}