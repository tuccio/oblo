#include <oblo/scene/scene_module.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/engine/engine_module.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/reflection/reflection_module.hpp>
#include <oblo/reflection/reflection_registry.hpp>
#include <oblo/reflection/registration/registrant.hpp>
#include <oblo/scene/assets/registration.hpp>
#include <oblo/scene/components/global_transform_component.hpp>
#include <oblo/scene/components/name_component.hpp>
#include <oblo/scene/components/position_component.hpp>
#include <oblo/scene/components/rotation_component.hpp>
#include <oblo/scene/components/scale_component.hpp>

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
                .add_field(&global_transform_component::value, "value")
                .add_ranged_type_erasure()
                .add_tag<ecs::component_type_tag>();
        }
    }

    bool scene_module::startup()
    {
        auto& mm = module_manager::get();

        auto* reflection = mm.load<reflection::reflection_module>();
        register_reflection(reflection->get_registrant());

        auto* const engineModule = module_manager::get().load<engine_module>();
        register_asset_types(engineModule->get_asset_registry());
        register_resource_types(engineModule->get_resource_registry());
        return true;
    }

    void scene_module::shutdown()
    {
        auto* const engineModule = module_manager::get().find<engine_module>();
        unregister_asset_types(engineModule->get_asset_registry());
        unregister_resource_types(engineModule->get_resource_registry());
    }
}