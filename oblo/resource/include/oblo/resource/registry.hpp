#pragma once

#include <oblo/core/handle.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/uuid.hpp>

#include <filesystem>
#include <unordered_map>
#include <vector>

namespace oblo
{
    struct type_id;
}

namespace oblo::resource
{
    template <typename T>
    class ptr;

    struct resource;
    struct type_desc;

    using find_resource_fn = bool (*)(const uuid& id, type_id& type, std::filesystem::path& path, const void* userdata);

    class registry
    {
    public:
        registry();
        registry(const registry&) = delete;
        registry(registry&&) noexcept = delete;
        ~registry();

        registry& operator=(const registry&) = delete;
        registry& operator=(registry&&) noexcept = delete;

        void register_type(const type_desc& typeDesc);
        void unregister_type(const type_id& type);

        void register_provider(find_resource_fn provider, const void* userdata);
        void unregister_provider(find_resource_fn provider);

        ptr<void> get_resource(const uuid& id);

    private:
        struct resource_storage;
        struct provider_storage;

    private:
        std::unordered_map<type_id, type_desc> m_resourceTypes;
        std::unordered_map<uuid, resource_storage> m_resources;
        std::vector<provider_storage> m_providers;
    };
}