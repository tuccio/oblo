#pragma once

#include <oblo/core/deque.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/resource/providers/resource_provider.hpp>
#include <oblo/resource/resource_traits.hpp>

#include <unordered_map>

namespace oblo
{
    class string;
    struct type_id;
}

namespace oblo
{
    template <typename T>
    class resource_ptr;

    struct resource;
    struct resource_type_descriptor;

    class resource_registry
    {
    public:
        resource_registry();
        resource_registry(const resource_registry&) = delete;
        resource_registry(resource_registry&&) noexcept = delete;
        ~resource_registry();

        resource_registry& operator=(const resource_registry&) = delete;
        resource_registry& operator=(resource_registry&&) noexcept = delete;

        void register_type(const resource_type_descriptor& typeDesc);
        void unregister_type(const uuid& type);

        void register_provider(resource_provider* provider);
        void unregister_provider(resource_provider* provider);

        resource_ptr<void> get_resource(const uuid& id) const;

        const deque<uuid>& get_updated_events(const uuid& eventType) const;

        template <typename T>
        const deque<uuid>& get_updated_events() const;

        void update();

    private:
        struct resource_storage;
        struct provider_storage;
        struct events_storage;

    private:
        std::unordered_map<uuid, resource_type_descriptor> m_resourceTypes;
        std::unordered_map<uuid, resource_storage> m_resources;
        std::unordered_map<uuid, events_storage> m_events;
        dynamic_array<provider_storage> m_providers;
        deque<uuid> m_noEvents;
    };

    template <typename T>
    const deque<uuid>& resource_registry::get_updated_events() const
    {
        return get_updated_events(resource_type<T>);
    }
}