#include <oblo/graphics/graphics_module.hpp>

#include <oblo/core/service_registry.hpp>
#include <oblo/core/service_registry_builder.hpp>
#include <oblo/core/struct_apply.hpp>
#include <oblo/ecs/services/world_builder.hpp>
#include <oblo/ecs/systems/system_graph_builder.hpp>
#include <oblo/graphics/components/camera_component.hpp>
#include <oblo/graphics/components/gpu_components.hpp>
#include <oblo/graphics/components/light_component.hpp>
#include <oblo/graphics/components/skybox_component.hpp>
#include <oblo/graphics/components/static_mesh_component.hpp>
#include <oblo/graphics/components/viewport_component.hpp>
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
#include <oblo/reflection/registration/module_registration.hpp>
#include <oblo/scene/reflection/gpu_component.hpp>
#include <oblo/scene/systems/barriers.hpp>
#include <oblo/vulkan/draw/draw_registry.hpp>
#include <oblo/vulkan/draw/resource_cache.hpp>
#include <oblo/vulkan/renderer.hpp>
#include <oblo/vulkan/vulkan_engine_module.hpp>

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

            reg.add_class<viewport_component>()
                .add_ranged_type_erasure()
                .add_tag<oblo::ecs::component_type_tag>()
                .add_field(&viewport_component::mode, "mode");

            reg.add_class<camera_component>()
                .add_ranged_type_erasure()
                .add_tag<oblo::ecs::component_type_tag>()
                .add_field(&camera_component::fovy, "fovy")
                .add_field(&camera_component::near, "near")
                .add_field(&camera_component::far, "far");

            reg.add_class<light_component>()
                .add_ranged_type_erasure()
                .add_tag<oblo::ecs::component_type_tag>()
                .add_field(&light_component::color, "color")
                .add_attribute<linear_color_tag>()
                .add_field(&light_component::intensity, "intensity")
                .add_field(&light_component::radius, "radius")
                .add_field(&light_component::type, "type")
                .add_field(&light_component::spotInnerAngle, "spotInnerAngle")
                .add_field(&light_component::spotOuterAngle, "spotOuterAngle")
                .add_field(&light_component::isShadowCaster, "isShadowCaster")
                .add_field(&light_component::hardShadows, "hardShadows")
                .add_field(&light_component::shadowBias, "shadowBias")
                .add_field(&light_component::shadowPunctualRadius, "shadowPunctualRadius")
                .add_field(&light_component::shadowDepthSigma, "shadowDepthSigma")
                .add_field(&light_component::shadowTemporalAccumulationFactor, "shadowTemporalAccumulationFactor")
                .add_field(&light_component::shadowMeanFilterSize, "shadowMeanFilterSize")
                .add_field(&light_component::shadowMeanFilterSigma, "shadowMeanFilterSigma");

            reg.add_enum<light_type>()
                .add_enumerator("point", light_type::point)
                .add_enumerator("spot", light_type::spot)
                .add_enumerator("directional", light_type::directional);

            reg.add_enum<viewport_mode>()
                .add_enumerator("lit", viewport_mode::lit)
                .add_enumerator("albedo", viewport_mode::albedo)
                .add_enumerator("normal_map", viewport_mode::normal_map)
                .add_enumerator("normals", viewport_mode::normals)
                .add_enumerator("tangents", viewport_mode::tangents)
                .add_enumerator("bitangents", viewport_mode::bitangents)
                .add_enumerator("uv0", viewport_mode::uv0)
                .add_enumerator("meslet", viewport_mode::meshlet)
                .add_enumerator("metalness", viewport_mode::metalness)
                .add_enumerator("roughness", viewport_mode::roughness)
                .add_enumerator("emissive", viewport_mode::emissive)
                .add_enumerator("motion_vectors", viewport_mode::motion_vectors)
                .add_enumerator("raytracing_debug", viewport_mode::raytracing_debug)
                .add_enumerator("gi_surfels", viewport_mode::gi_surfels)
                .add_enumerator("gi_surfels_lighting", viewport_mode::gi_surfels_lighting)
                .add_enumerator("gi_surfels_raycount", viewport_mode::gi_surfels_raycount)
                .add_enumerator("gi_surfels_inconsistency", viewport_mode::gi_surfels_inconsistency);

            reg.add_class<skybox_component>()
                .add_field(&skybox_component::texture, "texture")
                .add_field(&skybox_component::multiplier, "multiplier")
                .add_field(&skybox_component::tint, "tint")
                .add_attribute<linear_color_tag>()
                .add_ranged_type_erasure()
                .add_tag<ecs::component_type_tag>();

            reg.add_class<gpu_material>()
                .add_ranged_type_erasure()
                .add_concept(gpu_component{.bufferName = "i_MaterialBuffer"_hsv})
                .add_tag<ecs::component_type_tag>();

            reg.add_class<entity_id_component>()
                .add_concept(gpu_component{.bufferName = "i_EntityIdBuffer"_hsv})
                .add_ranged_type_erasure()
                .add_tag<ecs::component_type_tag>();
        }
    }

    bool graphics_module::startup(const module_initializer& initializer)
    {
        reflection::load_module_and_register(register_reflection);

        module_manager::get().load<vk::vulkan_engine_module>();

        initializer.services->add<ecs::world_builder>().unique({
            .services =
                [](service_registry_builder& builder)
            {
                builder.add<vk::renderer>().build(
                    [](service_builder<vk::renderer> builder)
                    {
                        auto* const vkEngine = module_manager::get().find<vk::vulkan_engine_module>();
                        builder.externally_owned(&vkEngine->get_renderer());
                    });

                builder.add<scene_renderer>().require<vk::renderer, vk::draw_registry>().build(
                    [](service_builder<scene_renderer> builder)
                    {
                        auto& renderer = *builder.find<vk::renderer>();
                        auto& drawRegistry = *builder.find<vk::draw_registry>();

                        builder.unique(renderer.get_frame_graph(), drawRegistry);
                    });

                builder.add<vk::resource_cache>().require<vk::renderer>().build(
                    [](service_builder<vk::resource_cache> builder)
                    {
                        auto& renderer = *builder.find<vk::renderer>();
                        builder.externally_owned(&renderer.get_resource_cache());
                    });

                builder.add<vk::draw_registry>()
                    .require<vk::renderer, ecs::entity_registry, const resource_registry>()
                    .build(
                        [](service_builder<vk::draw_registry> builder)
                        {
                            auto& renderer = *builder.find<vk::renderer>();
                            auto& entities = *builder.find<ecs::entity_registry>();
                            auto& resourceRegistry = *builder.find<const resource_registry>();

                            auto* drawRegistry = builder.unique();

                            drawRegistry->init(renderer.get_vulkan_context(),
                                renderer.get_staging_buffer(),
                                renderer.get_string_interner(),
                                entities,
                                resourceRegistry,
                                renderer.get_instance_data_type_registry());
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

        option_proxy_struct<surfels_gi_options>::register_options(*initializer.services);

        return true;
    }

    void graphics_module::shutdown() {}
}