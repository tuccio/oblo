#include <oblo/resource/resource_registry.hpp>

#include <oblo/resource/resource.hpp>
#include <oblo/resource/resource_ptr.hpp>
#include <oblo/resource/type_desc.hpp>

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
        find_resource_fn find;
        const void* userdata;
    };

    resource_registry::resource_registry() = default;

    resource_registry::~resource_registry() = default;

    void resource_registry::register_type(const resource_type_desc& typeDesc)
    {
        m_resourceTypes[typeDesc.type] = typeDesc;
    }

    void resource_registry::unregister_type(const type_id& type)
    {
        m_resourceTypes.erase(type);
    }

    void resource_registry::register_provider(find_resource_fn provider, const void* userdata)
    {
        m_providers.emplace_back(provider, userdata);
    }

    void resource_registry::unregister_provider(find_resource_fn provider)
    {
        const auto it = std::find_if(m_providers.begin(),
            m_providers.end(),
            [provider](const provider_storage& storage) { return storage.find == provider; });

        if (it != m_providers.end())
        {
            std::swap(*it, m_providers.back());
            m_providers.pop_back();
        }
    }

    resource_ptr<void> resource_registry::get_resource(const uuid& id)
    {
        const auto it = m_resources.find(id);

        if (it == m_resources.end())
        {
            type_id type;
            std::filesystem::path path;
            std::string name;
            bool anyFound{false};

            for (const auto [find, userdata] : m_providers)
            {
                if (find(id, type, name, path, userdata))
                {
                    anyFound = true;
                    break;
                }
            }

            if (!anyFound)
            {
                return {};
            }

            const auto typeIt = m_resourceTypes.find(type);

            if (typeIt == m_resourceTypes.end())
            {
                return {};
            }

            void* const data = typeIt->second.create();

            if (!typeIt->second.load(data, path))
            {
                typeIt->second.destroy(data);
                return {};
            }

            auto* const resource = detail::resource_create(data, type, id, name, typeIt->second.destroy);
            resource_ptr<void> handle{resource};
            m_resources.emplace(id, resource_storage{.resource = resource, .handle = handle});
            return handle;
        }

        return it->second.handle;
    }
}