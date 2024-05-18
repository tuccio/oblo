#pragma once

#include <oblo/modules/module_interface.hpp>

#include <memory>

namespace oblo
{
    class runtime_registry;

    class runtime_module final : public module_interface
    {
    public:
        static runtime_module& get();

    public:
        RUNTIME_API runtime_module();
        runtime_module(const runtime_module&) = delete;
        runtime_module(runtime_module&&) noexcept = delete;

        RUNTIME_API ~runtime_module();

        runtime_module& operator=(const runtime_module&) = delete;
        runtime_module& operator=(runtime_module&&) noexcept = delete;

        RUNTIME_API bool startup(const module_initializer& initializer) override;
        RUNTIME_API void shutdown() override;

        RUNTIME_API runtime_registry create_runtime_registry() const;

    private:
        struct impl;

    private:
        std::unique_ptr<impl> m_impl;
    };
}