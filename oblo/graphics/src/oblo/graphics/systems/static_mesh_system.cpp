#include <oblo/graphics/systems/static_mesh_system.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/frame_allocator.hpp>
#include <oblo/core/iterator/zip_range.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/ecs/range.hpp>
#include <oblo/ecs/systems/system_update_context.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/ecs/utility/deferred.hpp>
#include <oblo/ecs/utility/registration.hpp>
#include <oblo/graphics/components/static_mesh_component.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/resource/resource_ptr.hpp>
#include <oblo/resource/resource_ref.hpp>
#include <oblo/resource/resource_registry.hpp>
#include <oblo/scene/components/global_transform_component.hpp>
#include <oblo/scene/resources/material.hpp>
#include <oblo/scene/resources/pbr_properties.hpp>
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
            h32<vk::resident_texture> albedoTexture;
            f32 metalness;
            f32 roughness;
            h32<vk::resident_texture> metalnessRoughnessTexture;
            h32<vk::resident_texture> normalMapTexture;
            f32 ior;
            u32 _padding[3];
            vec3 emissive;
            h32<vk::resident_texture> emissiveTexture;
        };

        static_assert(sizeof(gpu_material) % 16 == 0);

        gpu_material convert(vk::resource_cache& cache, const material& m)
        {
            gpu_material out{};

            if (auto* const albedo = m.get_property(pbr::Albedo))
            {
                out.albedo = albedo->as<vec3>().value_or({});
            }

            if (auto* const metalness = m.get_property(pbr::Metalness))
            {
                out.metalness = metalness->as<f32>().value_or({});
            }

            if (auto* const roughness = m.get_property(pbr::Roughness))
            {
                out.roughness = roughness->as<f32>().value_or({});
            }

            if (auto* const emissive = m.get_property(pbr::Emissive))
            {
                out.emissive = emissive->as<vec3>().value_or({});
            }

            if (auto* const emissiveMultiplier = m.get_property(pbr::EmissiveMultiplier))
            {
                out.emissive = out.emissive * emissiveMultiplier->as<f32>().value_or(1.f);
            }

            if (auto* const albedoTexture = m.get_property(pbr::AlbedoTexture))
            {
                const auto t = albedoTexture->as<resource_ref<texture>>().value_or({});

                if (t)
                {
                    out.albedoTexture = cache.get_or_add(t);
                }
            }

            if (auto* const normalMapTexture = m.get_property(pbr::NormalMapTexture))
            {
                const auto t = normalMapTexture->as<resource_ref<texture>>().value_or({});

                if (t)
                {
                    out.normalMapTexture = cache.get_or_add(t);
                }
            }

            if (auto* const metalnessRoughnessTexture = m.get_property(pbr::MetalnessRoughnessTexture))
            {
                const auto t = metalnessRoughnessTexture->as<resource_ref<texture>>().value_or({});

                if (t)
                {
                    out.metalnessRoughnessTexture = cache.get_or_add(t);
                }
            }

            if (auto* const emissiveTexture = m.get_property(pbr::EmissiveTexture))
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

        struct mesh_resources
        {
            resource_ptr<material> material;
            resource_ptr<mesh> mesh;
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

        ecs::register_type<mesh_resources>(typeRegistry);

        drawRegistry.register_instance_data(gpuTransform, "i_TransformBuffer");
        drawRegistry.register_instance_data(gpuMaterial, "i_MaterialBuffer");
        drawRegistry.register_instance_data(entityId, "i_EntityIdBuffer");

        update(ctx);
    }

    void static_mesh_system::update(const ecs::system_update_context& ctx)
    {
        auto& drawRegistry = m_renderer->get_draw_registry();

        ecs::deferred deferred{ctx.frameAllocator};

        for (const auto [entities, meshComponents, globalTransforms] :
            ctx.entities->range<static_mesh_component, global_transform_component>().exclude<vk::draw_mesh_component>())
        {
            for (auto&& [entity, meshComponent, globalTransform] :
                zip_range(entities, meshComponents, globalTransforms))
            {
                auto materialRes = m_resourceRegistry->get_resource(meshComponent.material.id).as<material>();
                auto meshRes = m_resourceRegistry->get_resource(meshComponent.mesh.id).as<mesh>();

                if (!meshRes || !materialRes)
                {
                    // Maybe we should add a tag to avoid re-processing every frame
                    continue;
                }

                bool stillLoading = false;

                if (!materialRes.is_loaded())
                {
                    materialRes.load_start_async();
                    stillLoading = true;
                }

                // If the mesh is already on GPU we don't car about loading the resource
                auto mesh = drawRegistry.try_get_mesh(meshComponent.mesh);

                if (!mesh && !meshRes.is_loaded())
                {
                    meshRes.load_start_async();
                    stillLoading = true;
                }

                if (stillLoading)
                {
                    deferred.add<mesh_resources>(entity) = {
                        .material = std::move(materialRes),
                        .mesh = std::move(meshRes),
                    };

                    continue;
                }

                if (!mesh)
                {
                    mesh = drawRegistry.get_or_create_mesh(meshComponent.mesh);

                    if (!mesh)
                    {
                        continue;
                    }
                }

                auto&& [gpuMaterial, pickingId, gpuMeshComponent] =
                    deferred.add<gpu_material, entity_id_component, vk::draw_mesh_component, vk::draw_raytraced_tag>(
                        entity);

                deferred.remove<mesh_resources>(entity);

                gpuMaterial = convert(*m_resourceCache, *materialRes);

                pickingId.entityId = entity;

                gpuMeshComponent.mesh = mesh;
            }
        }

        deferred.apply(*ctx.entities);
    }
}