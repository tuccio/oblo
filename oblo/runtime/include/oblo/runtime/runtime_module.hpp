#pragma once

#include <oblo/core/unique_ptr.hpp>
#include <oblo/modules/module_interface.hpp>

namespace oblo
{
    class property_registry;
    class runtime_registry;

    class runtime_module final : public module_interface
    {
    public:
        static runtime_module& get();

    public:
        OBLO_RUNTIME_API runtime_module();
        runtime_module(const runtime_module&) = delete;
        runtime_module(runtime_module&&) noexcept = delete;

        OBLO_RUNTIME_API ~runtime_module();

        runtime_module& operator=(const runtime_module&) = delete;
        runtime_module& operator=(runtime_module&&) noexcept = delete;

        OBLO_RUNTIME_API bool startup(const module_initializer& initializer) override;
        OBLO_RUNTIME_API void shutdown() override;
        bool finalize() override;

        OBLO_RUNTIME_API runtime_registry create_runtime_registry() const;

        OBLO_RUNTIME_API const property_registry& get_property_registry() const;

    private:
        struct impl;

    private:
        unique_ptr<impl> m_impl;
    };
}