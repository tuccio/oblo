#include <oblo/scene/scene_module.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/ecs/systems/system_graph_builder.hpp>
#include <oblo/modules/module_initializer.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/reflection/reflection_module.hpp>
#include <oblo/reflection/reflection_registry.hpp>
#include <oblo/reflection/registration/registrant.hpp>
#include <oblo/resource/resource_types_provider.hpp>
#include <oblo/scene/assets/registration.hpp>
#include <oblo/scene/components/global_transform_component.hpp>
#include <oblo/scene/components/name_component.hpp>
#include <oblo/scene/components/position_component.hpp>
#include <oblo/scene/components/rotation_component.hpp>
#include <oblo/scene/components/scale_component.hpp>
#include <oblo/scene/systems/barriers.hpp>
#include <oblo/scene/systems/transform_system.hpp>

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
            reg.add_class<name_component>()
                .add_field(&name_component::value, "value")
                .add_ranged_type_erasure()
                .add_tag<ecs::component_type_tag>();

            reg.add_class<position_component>()
                .add_field(&position_component::value, "value")
                .add_ranged_type_erasure()
                .add_tag<ecs::component_type_tag>();

            reg.add_class<rotation_component>()
                .add_field(&rotation_component::value, "value")
                .add_ranged_type_erasure()
                .add_tag<ecs::component_type_tag>();

            reg.add_class<scale_component>()
                .add_field(&scale_component::value, "value")
                .add_ranged_type_erasure()
                .add_tag<ecs::component_type_tag>();

            reg.add_class<global_transform_component>()
                .add_field(&global_transform_component::localToWorld, "value")
                .add_ranged_type_erasure()
                .add_tag<ecs::component_type_tag>();
        }

        class scene_resources_provider final : public resource_types_provider
        {
            void fetch_resource_types(dynamic_array<resource_type_desc>& outResourceTypes) const override
            {
                fetch_scene_resource_types(outResourceTypes);
            }
        };
    }

    bool scene_module::startup(const module_initializer& initializer)
    {
        auto& mm = module_manager::get();

        auto* reflection = mm.load<reflection::reflection_module>();
        register_reflection(reflection->get_registrant());

        initializer.services->add<scene_resources_provider>().as<resource_types_provider>().unique();

        initializer.services->add<ecs::world_builder>().unique(
            [](ecs::system_graph_builder& b)
            {
                b.add_system<transform_system>().as<barriers::transform_update>();
                b.add_barrier<barriers::renderer_extract>().after<barriers::transform_update>();
                b.add_barrier<barriers::renderer_update>().after<barriers::renderer_extract>();
            });

        return true;
    }

    void scene_module::shutdown() {}
}