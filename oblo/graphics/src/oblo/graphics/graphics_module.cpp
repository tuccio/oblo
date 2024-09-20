#include <oblo/graphics/graphics_module.hpp>

#include <oblo/core/service_registry.hpp>
#include <oblo/ecs/services/world_builder.hpp>
#include <oblo/ecs/systems/system_graph_builder.hpp>
#include <oblo/graphics/components/camera_component.hpp>
#include <oblo/graphics/components/light_component.hpp>
#include <oblo/graphics/components/static_mesh_component.hpp>
#include <oblo/graphics/components/viewport_component.hpp>
#include <oblo/graphics/systems/lighting_system.hpp>
#include <oblo/graphics/systems/scene_renderer.hpp>
#include <oblo/graphics/systems/static_mesh_system.hpp>
#include <oblo/graphics/systems/viewport_system.hpp>
#include <oblo/math/color.hpp>
#include <oblo/modules/module_initializer.hpp>
#include <oblo/reflection/registration/module_registration.hpp>
#include <oblo/scene/systems/barriers.hpp>
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
                .add_field(&light_component::shadowSamples, "shadowSamples")
                .add_field(&light_component::shadowPunctualRadius, "shadowPunctualRadius")
                .add_field(&light_component::shadowTemporalAccumulationFactor, "shadowTemporalAccumulationFactor")
                .add_field(&light_component::shadowBlurKernel, "shadowBlurKernel")
                .add_field(&light_component::shadowBlurSigma, "shadowBlurSigma");

            reg.add_class<resource_ref<mesh>>().add_field(&resource_ref<mesh>::id, "id");
            reg.add_class<resource_ref<material>>().add_field(&resource_ref<material>::id, "id");

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
                .add_enumerator("raytracing_debug", viewport_mode::raytracing_debug);
        }
    }

    bool graphics_module::startup(const module_initializer& initializer)
    {
        reflection::load_module_and_register(register_reflection);

        initializer.services->add<ecs::world_builder>().unique({
            .services =
                [](service_registry& registry)
            {
                auto* const renderer = registry.find<vk::renderer>();
                registry.add<scene_renderer>().unique(renderer->get_frame_graph());
            },
            .systems =
                [](ecs::system_graph_builder& builder)
            {
                builder.add_system<lighting_system>()
                    .after<barriers::renderer_extract>()
                    .after<viewport_system>() // This way we can connect the shadow map graphs to the views
                    .before<barriers::renderer_update>();

                builder.add_system<viewport_system>()
                    .after<barriers::renderer_extract>()
                    .before<barriers::renderer_update>();

                builder.add_system<static_mesh_system>()
                    .after<barriers::renderer_extract>()
                    .before<barriers::renderer_update>();
            },
        });

        return true;
    }

    void graphics_module::shutdown() {}
}