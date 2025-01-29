#include <oblo/resource/resource_registry.hpp>

#include <oblo/core/string/string.hpp>
#include <oblo/resource/descriptors/resource_type_descriptor.hpp>
#include <oblo/resource/resource.hpp>
#include <oblo/resource/resource_ptr.hpp>

#include <algorithm>

namespace oblo
{
    struct resource_registry::resource_storage
    {
        resource* resource{nullptr};
        resource_ptr<void> handle;
    };

    struct resource_registry::provider_storage
    {
        resource_provider* provider;
    };

    resource_registry::resource_registry() = default;

    resource_registry::~resource_registry() = default;

    void resource_registry::register_type(const resource_type_descriptor& typeDesc)
    {
        m_resourceTypes[typeDesc.typeUuid] = typeDesc;
    }

    void resource_registry::unregister_type(const uuid& type)
    {
        m_resourceTypes.erase(type);
    }

    void resource_registry::register_provider(resource_provider* provider)
    {
        m_providers.emplace_back(provider);
    }

    void resource_registry::unregister_provider(resource_provider* provider)
    {
        const auto it = std::find_if(m_providers.begin(),
            m_providers.end(),
            [provider](const provider_storage& storage) { return storage.provider == provider; });

        if (it != m_providers.end())
        {
            std::swap(*it, m_providers.back());
            m_providers.pop_back();
        }
    }

    resource_ptr<void> resource_registry::get_resource(const uuid& id) const
    {
        resource_ptr<void> r;

        const auto it = m_resources.find(id);

        if (it != m_resources.end())
        {
            r = it->second.handle;
        }

        return r;
    }

    void resource_registry::update()
    {
        for (auto& provider : m_providers)
        {
            provider.provider->iterate_resource_events(
                [this](const resource_added_event& e)
                {
                    const auto typeIt = m_resourceTypes.find(e.typeUuid);

                    if (typeIt == m_resourceTypes.end())
                    {
                        return;
                    }

                    auto* const resource = detail::resource_create(&typeIt->second, e.id, e.name, e.path);

                    m_resources.emplace(e.id,
                        resource_storage{.resource = resource, .handle = resource_ptr<void>{resource}});
                },
                [this](const resource_removed_event& e) { m_resources.erase(e.id); });
        }
    }
}