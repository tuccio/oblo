#include <oblo/graphics/graphics_module.hpp>

#include <oblo/core/service_registry.hpp>
#include <oblo/ecs/services/service_registrant.hpp>
#include <oblo/graphics/components/camera_component.hpp>
#include <oblo/graphics/components/light_component.hpp>
#include <oblo/graphics/components/static_mesh_component.hpp>
#include <oblo/graphics/components/viewport_component.hpp>
#include <oblo/graphics/systems/scene_renderer.hpp>
#include <oblo/modules/module_initializer.hpp>
#include <oblo/reflection/registration/module_registration.hpp>
#include <oblo/vulkan/renderer.hpp>

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

            reg.add_class<light_component>()
                .add_field(&light_component::color, "color")
                .add_field(&light_component::intensity, "intensity")
                .add_field(&light_component::radius, "radius")
                .add_field(&light_component::type, "type")
                .add_field(&light_component::spotInnerAngle, "spotInnerAngle")
                .add_field(&light_component::spotOuterAngle, "spotOuterAngle")
                .add_ranged_type_erasure()
                .add_tag<oblo::ecs::component_type_tag>();

            reg.add_class<resource_ref<mesh>>().add_field(&resource_ref<mesh>::id, "id");
            reg.add_class<resource_ref<material>>().add_field(&resource_ref<material>::id, "id");
        }
    }

    bool graphics_module::startup(const module_initializer& initializer)
    {
        reflection::load_module_and_register(register_reflection);

        initializer.services->add<ecs::service_registrant>().unique(
            [](service_registry& registry)
            {
                auto* const renderer = registry.find<vk::renderer>();
                registry.add<scene_renderer>().unique(renderer->get_frame_graph());
            });

        return true;
    }

    void graphics_module::shutdown() {}
}