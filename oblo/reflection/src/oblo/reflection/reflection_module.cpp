#include <oblo/reflection/reflection_module.hpp>

#include <oblo/reflection/registration/registrant.hpp>

#include <oblo/math/vec3.hpp>

namespace oblo::reflection
{
    namespace
    {
        void register_reflection(reflection_registry::registrant registrant)
        {
            registrant.add_class<vec3>().add_field(&vec3::x, "x").add_field(&vec3::y, "y").add_field(&vec3::z, "z");
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