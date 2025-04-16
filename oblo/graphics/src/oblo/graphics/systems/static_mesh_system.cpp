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
#include <oblo/graphics/components/gpu_components.hpp>
#include <oblo/graphics/components/static_mesh_component.hpp>
#include <oblo/graphics/components/static_mesh_internal.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/resource/resource_ptr.hpp>
#include <oblo/resource/resource_ref.hpp>
#include <oblo/resource/resource_registry.hpp>
#include <oblo/scene/components/global_transform_component.hpp>
#include <oblo/scene/resources/material.hpp>
#include <oblo/scene/resources/pbr_properties.hpp>
#include <oblo/vulkan/data/components.hpp>
#include <oblo/vulkan/draw/draw_registry.hpp>
#include <oblo/vulkan/draw/resource_cache.hpp>
#include <oblo/vulkan/renderer.hpp>

#include <span>

namespace oblo
{
    namespace
    {
        gpu_material convert(const resource_registry& resources, vk::resource_cache& cache, const material& m)
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
                    out.albedoTexture = cache.get_or_add(resources.get_resource(t));
                }
            }

            if (auto* const normalMapTexture = m.get_property(pbr::NormalMapTexture))
            {
                const auto t = normalMapTexture->as<resource_ref<texture>>().value_or({});

                if (t)
                {
                    out.normalMapTexture = cache.get_or_add(resources.get_resource(t));
                }
            }

            if (auto* const metalnessRoughnessTexture = m.get_property(pbr::MetalnessRoughnessTexture))
            {
                const auto t = metalnessRoughnessTexture->as<resource_ref<texture>>().value_or({});

                if (t)
                {
                    out.metalnessRoughnessTexture = cache.get_or_add(resources.get_resource(t));
                }
            }

            if (auto* const emissiveTexture = m.get_property(pbr::EmissiveTexture))
            {
                const auto t = emissiveTexture->as<resource_ref<texture>>().value_or({});

                if (t)
                {
                    out.emissiveTexture = cache.get_or_add(resources.get_resource(t));
                }
            }

            return out;
        }

        void add_mesh(const resource_registry* resourceRegistry,
            vk::resource_cache* resourceCache,
            vk::draw_registry& drawRegistry,
            ecs::entity entity,
            const static_mesh_component& meshComponent,
            ecs::deferred& deferred)
        {
            auto materialRes = resourceRegistry->get_resource(meshComponent.material.id).as<material>();
            auto meshRes = resourceRegistry->get_resource(meshComponent.mesh.id).as<mesh>();

            if (!meshRes || !materialRes)
            {
                // Maybe we should add a tag to avoid re-processing every frame
                return;
            }

            bool stillLoading = false;

            if (!materialRes.is_loaded())
            {
                materialRes.load_start_async();
                stillLoading = true;
            }

            // If the mesh is already on GPU we don't care about loading the resource
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

                return;
            }

            if (!mesh)
            {
                mesh = drawRegistry.get_or_create_mesh(meshComponent.mesh);

                if (!mesh)
                {
                    deferred.add<mesh_processed_tag>(entity);
                    return;
                }
            }

            auto&& [gpuMaterial, pickingId, cachedRefs, gpuMeshComponent] = deferred.add<gpu_material,
                entity_id_component,
                processed_mesh_resources,
                vk::draw_mesh_component,
                vk::draw_raytraced_tag,
                mesh_processed_tag>(entity);

            deferred.remove<mesh_resources>(entity);

            gpuMaterial = convert(*resourceRegistry, *resourceCache, *materialRes);

            pickingId.entityId = entity;

            gpuMeshComponent.mesh = mesh;

            // Store a checksum so we can determine whether or not we want to re-process it
            cachedRefs = processed_mesh_resources::from(meshComponent);
        }
    }

    void static_mesh_system::first_update(const ecs::system_update_context& ctx)
    {
        m_drawRegistry = ctx.services->find<vk::draw_registry>();
        OBLO_ASSERT(m_drawRegistry);

        m_resourceRegistry = ctx.services->find<const resource_registry>();
        OBLO_ASSERT(m_resourceRegistry);

        m_resourceCache = ctx.services->find<vk::resource_cache>();
        OBLO_ASSERT(m_resourceCache);

        update(ctx);
    }

    void static_mesh_system::update(const ecs::system_update_context& ctx)
    {
        ecs::deferred deferred{ctx.frameAllocator};

        if (!m_resourceRegistry->get_updated_events<material>().empty())
        {
            // Just invalidate all entities that we already processed, instead of trying to figure which one use the
            // resources that were invalidated
            for (auto&& chunk : ctx.entities->range<const static_mesh_component>()
                     .with<global_transform_component, mesh_processed_tag>())
            {
                for (auto&& e : chunk.get<ecs::entity>())
                {
                    deferred.remove<mesh_processed_tag>(e);
                }
            }
        }

        // Process entities we already processed, in order to react to changes
        for (auto&& chunk : ctx.entities->range<const processed_mesh_resources, const static_mesh_component>()
                 .with<mesh_processed_tag>()
                 .notified())
        {
            for (auto&& [e, cachedRefs, meshComponent] :
                chunk.zip<ecs::entity, processed_mesh_resources, static_mesh_component>())
            {
                if (processed_mesh_resources::from(meshComponent) != cachedRefs)
                {
                    deferred.remove<mesh_processed_tag>(e);
                }
            }
        }

        deferred.apply(*ctx.entities);

        // Process entities that we didn't process yet or we just invalidated
        for (auto&& chunk : ctx.entities->range<const static_mesh_component>()
                 .with<global_transform_component>()
                 .exclude<mesh_processed_tag>())
        {
            for (auto&& [e, meshComponent] : chunk.zip<ecs::entity, static_mesh_component>())
            {
                add_mesh(m_resourceRegistry, m_resourceCache, *m_drawRegistry, e, meshComponent, deferred);
            }
        }

        deferred.apply(*ctx.entities);
    }
}