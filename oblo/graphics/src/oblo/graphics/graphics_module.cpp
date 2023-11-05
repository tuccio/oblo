#include <oblo/graphics/graphics_module.hpp>

#include <oblo/graphics/components/static_mesh_component.hpp>
#include <oblo/graphics/components/viewport_component.hpp>
#include <oblo/reflection/registration/module_registration.hpp>

namespace oblo::ecs
{
    struct component_type_tag;
}

namespace oblo::graphics
{
    namespace
    {
        void register_reflection(reflection::reflection_registry::registrant reg)
        {
            reg.add_class<static_mesh_component>()
                .add_ranged_type_erasure()
                .add_tag<oblo::ecs::component_type_tag>()
                .add_field(&static_mesh_component::mesh, "mesh");

            reg.add_class<viewport_component>().add_ranged_type_erasure().add_tag<oblo::ecs::component_type_tag>();
        }
    }

    bool graphics_module::startup()
    {
        reflection::load_module_and_register(register_reflection);
        return true;
    }

    void graphics_module::shutdown()
    {
        //
    }
}