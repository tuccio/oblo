#include <oblo/resource/resource_registry.hpp>

#include <oblo/core/string/string.hpp>
#include <oblo/core/uuid_generator.hpp>
#include <oblo/resource/descriptors/resource_type_descriptor.hpp>
#include <oblo/resource/resource.hpp>
#include <oblo/resource/resource_ptr.hpp>

#include <algorithm>

namespace oblo
{
    struct resource_registry::resource_storage
    {
        resource_ptr<void> handle;
    };

    struct resource_registry::provider_storage
    {
        resource_provider* provider;
    };

    struct resource_registry::events_storage
    {
        deque<uuid> updatedEvents;
    };

    resource_registry::resource_registry() = default;

    resource_registry::~resource_registry() = default;

    void resource_registry::register_type(resource_type_descriptor typeDesc)
    {
        m_resourceTypes[typeDesc.typeUuid] = std::move(typeDesc);
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

    resource_ptr<void> resource_registry::instantiate(
        const uuid& type, function_ref<void(void*)> init, string_view name) const
    {
        resource_ptr<void> r;

        const auto it = m_resourceTypes.find(type);

        if (it != m_resourceTypes.end())
        {
            auto* const resource = detail::resource_create(&it->second, uuid_system_generator{}.generate(), name, {});
            r = resource_ptr<void>{resource};

            if (!detail::resource_instantiate(resource))
            {
                r.reset();
            }

            init(resource->data);
        }

        return r;
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
        m_events.clear();

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
                        resource_storage{
                            .handle = resource_ptr<void>{resource},
                        });
                },
                [this](const resource_removed_event& e)
                {
                    const auto it = m_resources.find(e.id);

                    if (it != m_resources.end())
                    {
                        if (it->second.handle)
                        {
                            it->second.handle.invalidate();
                        }

                        m_resources.erase(it);
                    }
                },
                [this](const resource_updated_event& e)
                {
                    const auto typeIt = m_resourceTypes.find(e.typeUuid);

                    if (typeIt == m_resourceTypes.end())
                    {
                        return;
                    }

                    if (!m_resources.contains(e.id))
                    {
                        return;
                    }

                    auto* const resource = detail::resource_create(&typeIt->second, e.id, e.name, e.path);

                    auto& entry = m_resources[e.id];

                    if (entry.handle)
                    {
                        entry.handle.invalidate();
                    }

                    entry = resource_storage{
                        .handle = resource_ptr<void>{resource},
                    };

                    m_events[e.typeUuid].updatedEvents.push_back(e.id);
                });
        }
    }

    const deque<uuid>& resource_registry::get_updated_events(const uuid& eventType) const
    {
        const auto it = m_events.find(eventType);

        if (it == m_events.end())
        {
            return m_noEvents;
        }

        return it->second.updatedEvents;
    }
}
