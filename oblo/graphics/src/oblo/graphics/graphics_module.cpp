#include <oblo/graphics/graphics_module.hpp>

#include <oblo/graphics/components/camera_component.hpp>
#include <oblo/graphics/components/static_mesh_component.hpp>
#include <oblo/graphics/components/viewport_component.hpp>
#include <oblo/reflection/registration/module_registration.hpp>

namespace oblo::ecs
{
    struct component_type_tag;
}

namespace oblo
{
    namespace
    {
        void register_reflection(reflection::reflection_registry::registrant reg)
        {
            reg.add_class<static_mesh_component>()
                .add_field(&static_mesh_component::mesh, "mesh")
                .add_field(&static_mesh_component::material, "material")
                .add_ranged_type_erasure()
                .add_tag<oblo::ecs::component_type_tag>();

            reg.add_class<viewport_component>().add_ranged_type_erasure().add_tag<oblo::ecs::component_type_tag>();

            reg.add_class<camera_component>()
                .add_field(&camera_component::fovy, "fovy")
                .add_field(&camera_component::near, "near")
                .add_field(&camera_component::far, "far")
                .add_ranged_type_erasure()
                .add_tag<oblo::ecs::component_type_tag>();

            reg.add_class<resource_ref<mesh>>().add_field(&resource_ref<mesh>::id, "id");
            reg.add_class<resource_ref<material>>().add_field(&resource_ref<material>::id, "id");
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