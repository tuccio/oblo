#include <oblo/reflection/reflection_module.hpp>

#include <oblo/reflection/registration/common.hpp>
#include <oblo/reflection/registration/registrant.hpp>

namespace oblo::reflection
{
    namespace
    {
        void register_reflection(reflection_registry::registrant registrant)
        {
            register_fundamental_types(registrant);
            register_math_types(registrant);
        }
    }

    bool reflection_module::startup()
    {
        register_reflection(m_registry.get_registrant());
        return true;
    }

    void reflection_module::shutdown() {}

    const reflection_registry& reflection_module::get_registry() const
    {
        return m_registry;
    }

    reflection_registry::registrant reflection_module::get_registrant()
    {
        return m_registry.get_registrant();
    }
}