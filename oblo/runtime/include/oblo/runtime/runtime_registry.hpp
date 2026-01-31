#pragma once

#include <oblo/core/unique_ptr.hpp>
#include <oblo/modules/module_interface.hpp>

namespace oblo
{
    class property_registry;
    class resource_registry;
    class runtime_module;

    class runtime_registry
    {
    public:
        OBLO_RUNTIME_API runtime_registry();
        runtime_registry(const runtime_registry&) = delete;
        OBLO_RUNTIME_API runtime_registry(runtime_registry&&) noexcept;

        OBLO_RUNTIME_API ~runtime_registry();

        runtime_registry& operator=(const runtime_registry&) = delete;
        OBLO_RUNTIME_API runtime_registry& operator=(runtime_registry&&) noexcept;

        OBLO_RUNTIME_API property_registry& get_property_registry();

        OBLO_RUNTIME_API resource_registry& get_resource_registry();

        OBLO_RUNTIME_API void shutdown();

    private:
        explicit runtime_registry(property_registry* propertyRegistry);

    private:
        friend class runtime_module;
        struct impl;

    private:
        unique_ptr<impl> m_impl;
    };
}