#include <oblo/graphics/systems/static_mesh_system.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/iterator/zip_range.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/ecs/range.hpp>
#include <oblo/ecs/systems/system_update_context.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/ecs/utility/registration.hpp>
#include <oblo/graphics/components/static_mesh_component.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/resource/resource_ptr.hpp>
#include <oblo/resource/resource_ref.hpp>
#include <oblo/resource/resource_registry.hpp>
#include <oblo/scene/assets/material.hpp>
#include <oblo/scene/assets/pbr_properties.hpp>
#include <oblo/scene/components/global_transform_component.hpp>
#include <oblo/vulkan/draw/draw_registry.hpp>
#include <oblo/vulkan/draw/resource_cache.hpp>
#include <oblo/vulkan/renderer.hpp>

#include <span>

namespace oblo
{
    namespace
    {
        struct gpu_material
        {
            vec3 albedo;
            f32 metalness;
            f32 roughness;
            f32 emissive;
            h32<vk::resident_texture> albedoTexture;
            h32<vk::resident_texture> normalMapTexture;
            h32<vk::resident_texture> metalnessRoughnessTexture;
            h32<vk::resident_texture> emissiveTexture;
        };

        gpu_material convert(vk::resource_cache& cache, resource_ptr<material> m)
        {
            gpu_material out{};

            if (!m)
            {
                return out;
            }

            if (auto* const albedo = m->get_property(pbr::Albedo))
            {
                out.albedo = albedo->as<vec3>().value_or({});
            }

            if (auto* const metalness = m->get_property(pbr::Metalness))
            {
                out.metalness = metalness->as<f32>().value_or({});
            }

            if (auto* const roughness = m->get_property(pbr::Roughness))
            {
                out.roughness = roughness->as<f32>().value_or({});
            }

            if (auto* const emissive = m->get_property(pbr::Emissive))
            {
                out.emissive = emissive->as<f32>().value_or({});
            }

            if (auto* const albedoTexture = m->get_property(pbr::AlbedoTexture))
            {
                const auto t = albedoTexture->as<resource_ref<texture>>().value_or({});

                if (t)
                {
                    out.albedoTexture = cache.get_or_add(t);
                }
            }

            if (auto* const normalMapTexture = m->get_property(pbr::NormalMapTexture))
            {
                const auto t = normalMapTexture->as<resource_ref<texture>>().value_or({});

                if (t)
                {
                    out.normalMapTexture = cache.get_or_add(t);
                }
            }

            if (auto* const metalnessRoughnessTexture = m->get_property(pbr::MetalnessRoughnessTexture))
            {
                const auto t = metalnessRoughnessTexture->as<resource_ref<texture>>().value_or({});

                if (t)
                {
                    out.metalnessRoughnessTexture = cache.get_or_add(t);
                }
            }

            if (auto* const emissiveTexture = m->get_property(pbr::EmissiveTexture))
            {
                const auto t = emissiveTexture->as<resource_ref<texture>>().value_or({});

                if (t)
                {
                    out.emissiveTexture = cache.get_or_add(t);
                }
            }

            return out;
        }

        struct entity_id_component
        {
            ecs::entity entityId;
        };
    }

    void static_mesh_system::first_update(const ecs::system_update_context& ctx)
    {
        m_renderer = ctx.services->find<vk::renderer>();
        OBLO_ASSERT(m_renderer);

        m_resourceRegistry = ctx.services->find<resource_registry>();
        OBLO_ASSERT(m_resourceRegistry);

        m_resourceCache = ctx.services->find<vk::resource_cache>();
        OBLO_ASSERT(m_resourceCache);

        auto& drawRegistry = m_renderer->get_draw_registry();

        auto& typeRegistry = ctx.entities->get_type_registry();

        const auto gpuTransform = typeRegistry.find_component<global_transform_component>();
        const auto gpuMaterial = typeRegistry.register_component(ecs::make_component_type_desc<gpu_material>());
        const auto entityId = typeRegistry.register_component(ecs::make_component_type_desc<entity_id_component>());

        drawRegistry.register_instance_data(gpuTransform, "i_TransformBuffer");
        drawRegistry.register_instance_data(gpuMaterial, "i_MaterialBuffer");
        drawRegistry.register_instance_data(entityId, "i_EntityIdBuffer");

        update(ctx);
    }

    void static_mesh_system::update(const ecs::system_update_context& ctx)
    {
        auto& drawRegistry = m_renderer->get_draw_registry();

        struct deferred_creation
        {
            ecs::entity e;
            h32<vk::draw_mesh> mesh;
            resource_ref<material> material;
        };

        // TODO: (#7) Implement a way to create components while iterating, or deferring
        dynamic_array<deferred_creation> deferred;

        for (const auto [entities, meshComponents, globalTransforms] :
            ctx.entities->range<static_mesh_component, global_transform_component>().exclude<vk::draw_mesh_component>())
        {
            for (auto&& [entity, meshComponent, globalTransform] :
                zip_range(entities, meshComponents, globalTransforms))
            {
                const auto mesh = drawRegistry.get_or_create_mesh(*m_resourceRegistry, meshComponent.mesh);

                if (!mesh)
                {
                    continue;
                }

                deferred.push_back_default() = {
                    .e = entity,
                    .mesh = mesh,
                    .material = meshComponent.material,
                };
            }
        }

        // Finally create components if necessary
        for (const auto& [e, mesh, materialRef] : deferred)
        {
            auto&& [gpuMaterial, pickingId, meshComponent] =
                ctx.entities->add<gpu_material, entity_id_component, vk::draw_mesh_component>(e);

            gpuMaterial = convert(*m_resourceCache, m_resourceRegistry->get_resource(materialRef.id).as<material>());
            pickingId.entityId = e;
            meshComponent.mesh = mesh;
        }
    }
}