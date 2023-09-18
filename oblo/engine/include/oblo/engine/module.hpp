#pragma once

#include <oblo/modules/module_interface.hpp>

#include <memory>

namespace oblo::asset
{
    class asset_registry;
}

namespace oblo::resource
{
    class resource_registry;
}

namespace oblo::engine
{
    class ENGINE_API module final : public module_interface
    {
    public:
        static module& get();

    public:
        module();
        module(const module&) = delete;
        module(module&&) noexcept = delete;

        ~module();

        module& operator=(const module&) = delete;
        module& operator=(module&&) noexcept = delete;

        bool startup() override;
        void shutdown() override;

        asset::asset_registry& get_asset_registry() const
        {
            return *m_assetRegistry;
        }

        resource::resource_registry& get_resource_registry() const
        {
            return *m_resourceregistry;
        }

    private:
        std::unique_ptr<asset::asset_registry> m_assetRegistry;
        std::unique_ptr<resource::resource_registry> m_resourceregistry;
    };
}