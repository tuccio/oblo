#pragma once

#include <oblo/modules/module_interface.hpp>

#include <memory>

namespace oblo
{
    class property_registry;
    class resource_registry;
    class runtime_module;

    class runtime_registry
    {
    public:
        RUNTIME_API runtime_registry();
        runtime_registry(const runtime_registry&) = delete;
        RUNTIME_API runtime_registry(runtime_registry&&) noexcept;

        RUNTIME_API ~runtime_registry();

        runtime_registry& operator=(const runtime_registry&) = delete;
        RUNTIME_API runtime_registry& operator=(runtime_registry&&) noexcept;

        RUNTIME_API property_registry& get_property_registry();

        RUNTIME_API resource_registry& get_resource_registry();

    private:
        explicit runtime_registry(property_registry* propertyRegistry);

    private:
        friend class runtime_module;
        struct impl;

    private:
        std::unique_ptr<impl> m_impl;
    };
}