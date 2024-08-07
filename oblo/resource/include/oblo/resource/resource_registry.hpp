#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/uuid.hpp>

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
    struct resource_type_desc;

    using find_resource_fn = bool (*)(
        const uuid& id, type_id& outType, string& outName, string& outPath, const void* userdata);

    class resource_registry
    {
    public:
        resource_registry();
        resource_registry(const resource_registry&) = delete;
        resource_registry(resource_registry&&) noexcept = delete;
        ~resource_registry();

        resource_registry& operator=(const resource_registry&) = delete;
        resource_registry& operator=(resource_registry&&) noexcept = delete;

        void register_type(const resource_type_desc& typeDesc);
        void unregister_type(const type_id& type);

        void register_provider(find_resource_fn provider, const void* userdata);
        void unregister_provider(find_resource_fn provider);

        resource_ptr<void> get_resource(const uuid& id);

    private:
        struct resource_storage;
        struct provider_storage;

    private:
        std::unordered_map<type_id, resource_type_desc> m_resourceTypes;
        std::unordered_map<uuid, resource_storage> m_resources;
        dynamic_array<provider_storage> m_providers;
    };
}