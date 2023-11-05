#pragma once

#include <oblo/modules/module_interface.hpp>

#include <oblo/reflection/reflection_registry.hpp>

namespace oblo::reflection
{
    class reflection_module final : public module_interface
    {
    public:
        bool startup() override;
        void shutdown() override;

        const reflection_registry& get_registry() const;

        reflection_registry::registrant get_registrant();

    private:
        reflection_registry m_registry;
    };
}