#pragma once

#include <oblo/modules/module_interface.hpp>

#include <memory>

namespace oblo
{
    class asset_registry;
}

namespace oblo
{
    class resource_registry;
}

namespace oblo
{
    class ENGINE_API engine_module final : public module_interface
    {
    public:
        static engine_module& get();

    public:
        engine_module();
        engine_module(const engine_module&) = delete;
        engine_module(engine_module&&) noexcept = delete;

        ~engine_module();

        engine_module& operator=(const engine_module&) = delete;
        engine_module& operator=(engine_module&&) noexcept = delete;

        bool startup() override;
        void shutdown() override;

        asset_registry& get_asset_registry() const
        {
            return *m_assetRegistry;
        }

        resource_registry& get_resource_registry() const
        {
            return *m_resourceRegistry;
        }

    private:
        std::unique_ptr<asset_registry> m_assetRegistry;
        std::unique_ptr<resource_registry> m_resourceRegistry;
    };
}