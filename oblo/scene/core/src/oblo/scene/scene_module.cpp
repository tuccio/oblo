#include <oblo/scene/scene_module.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/ecs/services/world_builder.hpp>
#include <oblo/ecs/systems/system_graph_builder.hpp>
#include <oblo/modules/module_initializer.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/reflection/codegen/registration.hpp>
#include <oblo/reflection/concepts/gpu_component.hpp>
#include <oblo/reflection/reflection_module.hpp>
#include <oblo/reflection/reflection_registry.hpp>
#include <oblo/resource/providers/resource_types_provider.hpp>
#include <oblo/scene/resources/registration.hpp>
#include <oblo/scene/systems/barriers.hpp>
#include <oblo/scene/systems/entity_hierarchy_system.hpp>
#include <oblo/scene/systems/transform_system.hpp>

namespace oblo::ecs
{
    struct component_type_tag;
    struct tag_type_tag;
}

namespace oblo
{
    namespace
    {
        class scene_resources_provider final : public resource_types_provider
        {
            void fetch_resource_types(deque<resource_type_descriptor>& outResourceTypes) const override
            {
                fetch_scene_resource_types(outResourceTypes);
            }
        };
    }

    bool scene_module::startup(const module_initializer& initializer)
    {
        reflection::gen::load_module_and_register();

        initializer.services->add<scene_resources_provider>().as<resource_types_provider>().unique();

        initializer.services->add<ecs::world_builder>().unique({
            .systems =
                [](ecs::system_graph_builder& b)
            {
                b.add_system<transform_system>().as<barriers::transform_update>();
                b.add_barrier<barriers::renderer_extract>().after<barriers::transform_update>();
                b.add_barrier<barriers::renderer_update>().after<barriers::renderer_extract>();
                b.add_system<entity_hierarchy_system>().before<barriers::transform_update>();
            },
        });

        return true;
    }

    void scene_module::shutdown() {}
}