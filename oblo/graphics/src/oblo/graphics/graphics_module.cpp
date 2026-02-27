#include <oblo/graphics/graphics_module.hpp>

#include <oblo/core/service_registry.hpp>
#include <oblo/core/service_registry_builder.hpp>
#include <oblo/core/struct_apply.hpp>
#include <oblo/ecs/services/world_builder.hpp>
#include <oblo/ecs/systems/system_graph_builder.hpp>
#include <oblo/graphics/services/scene_renderer.hpp>
#include <oblo/graphics/systems/draw_registry_system.hpp>
#include <oblo/graphics/systems/graphics_options.hpp>
#include <oblo/graphics/systems/lighting_system.hpp>
#include <oblo/graphics/systems/skybox_system.hpp>
#include <oblo/graphics/systems/static_mesh_system.hpp>
#include <oblo/graphics/systems/viewport_system.hpp>
#include <oblo/math/color.hpp>
#include <oblo/modules/module_initializer.hpp>
#include <oblo/options/options_module.hpp>
#include <oblo/reflection/codegen/registration.hpp>
#include <oblo/renderer/draw/draw_registry.hpp>
#include <oblo/renderer/draw/resource_cache.hpp>
#include <oblo/renderer/renderer.hpp>
#include <oblo/scene/systems/barriers.hpp>
#include <oblo/vulkan/vulkan_engine_module.hpp>

namespace oblo
{
    bool graphics_module::startup(const module_initializer& initializer)
    {
        reflection::gen::load_module_and_register();

        module_manager::get().load<vk::vulkan_engine_module>();

        initializer.services->add<ecs::world_builder>().unique({
            .services =
                [](service_registry_builder& builder)
            {
                builder.add<renderer>().build(
                    [](service_builder<renderer> builder)
                    {
                        auto* const vkEngine = module_manager::get().find<vk::vulkan_engine_module>();
                        builder.externally_owned(&vkEngine->get_renderer());
                    });

                builder.add<scene_renderer>().require<renderer, draw_registry>().build(
                    [](service_builder<scene_renderer> builder)
                    {
                        auto& r = *builder.find<renderer>();
                        auto& drawRegistry = *builder.find<draw_registry>();

                        builder.unique(r.get_frame_graph(), drawRegistry);
                    });

                builder.add<resource_cache>().require<renderer>().build(
                    [](service_builder<resource_cache> builder)
                    {
                        auto& r = *builder.find<renderer>();
                        builder.externally_owned(&r.get_resource_cache());
                    });

                builder.add<draw_registry>().require<renderer, ecs::entity_registry, const resource_registry>().build(
                    [](service_builder<draw_registry> builder)
                    {
                        auto& r = *builder.find<renderer>();
                        auto& entities = *builder.find<ecs::entity_registry>();
                        auto& resourceRegistry = *builder.find<const resource_registry>();

                        auto* drawRegistry = builder.unique();

                        drawRegistry->init(r.get_gpu_instance(),
                            r.get_staging_buffer(),
                            r.get_string_interner(),
                            entities,
                            resourceRegistry,
                            r.get_instance_data_type_registry());
                    });
            },
            .systems =
                [](ecs::system_graph_builder& builder)
            {
                builder.add_system<lighting_system>()
                    .after<barriers::renderer_extract>()
                    .before<barriers::renderer_update>();

                builder.add_system<viewport_system>()
                    .after<barriers::renderer_extract>()
                    .before<barriers::renderer_update>();

                builder.add_system<static_mesh_system>()
                    .after<barriers::renderer_extract>()
                    .before<barriers::renderer_update>();

                builder.add_system<skybox_system>()
                    .after<barriers::renderer_extract>()
                    .before<barriers::renderer_update>();

                builder.add_system<draw_registry_system>().after<barriers::renderer_update>();
            },
        });

        option_proxy_struct<surfels_gi_options, rtao_options>::register_options(*initializer.services);

        return true;
    }

    void graphics_module::shutdown() {}
}