#include <oblo/graphics/systems/static_mesh_system.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/frame_allocator.hpp>
#include <oblo/core/iterator/zip_range.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/core/span.hpp>
#include <oblo/ecs/range.hpp>
#include <oblo/ecs/systems/system_update_context.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/ecs/utility/deferred.hpp>
#include <oblo/graphics/components/gpu_components.hpp>
#include <oblo/graphics/components/mesh_internal.hpp>
#include <oblo/graphics/components/skin_component.hpp>
#include <oblo/graphics/components/static_mesh_component.hpp>
#include <oblo/log/log.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/renderer/data/components.hpp>
#include <oblo/renderer/draw/draw_registry.hpp>
#include <oblo/renderer/draw/resource_cache.hpp>
#include <oblo/resource/resource_ptr.hpp>
#include <oblo/resource/resource_ref.hpp>
#include <oblo/resource/resource_registry.hpp>
#include <oblo/scene/components/children_component.hpp>
#include <oblo/scene/components/global_transform_component.hpp>
#include <oblo/scene/components/parent_component.hpp>
#include <oblo/scene/components/tags.hpp>
#include <oblo/scene/resources/material.hpp>
#include <oblo/scene/resources/pbr_properties.hpp>
#include <oblo/scene/resources/skeleton.hpp>

namespace oblo
{
    namespace
    {
        gpu_material convert(const resource_registry& resources, resource_cache& cache, const material& m)
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

        template <bool WithSkin>
        bool try_add_mesh(const resource_registry* resourceRegistry,
            resource_cache* resourceCache,
            draw_registry& drawRegistry,
            ecs::entity entity,
            const static_mesh_component& meshComponent,
            [[maybe_unused]] const skin_component* skinComponent,
            ecs::deferred& deferred)
        {
            auto materialRes = resourceRegistry->get_resource(meshComponent.material);
            auto meshRes = resourceRegistry->get_resource(meshComponent.mesh);

            if (!meshRes || !materialRes)
            {
                // Maybe we should add a tag to avoid re-processing every frame
                log::debug("Failed to find mesh or material for entity {}", entity.value);
                return false;
            }

            bool stillLoading = false;

            resource_ptr<skin> skinRes{};
            resource_ptr<skeleton> skeletonRes{};

            if constexpr (WithSkin)
            {
                skinRes = resourceRegistry->get_resource(skinComponent->skin);

                if (!skinRes)
                {
                    log::debug("Failed to find skin resource for entity {}", entity.value);
                    return false;
                }

                skinRes.load_start_async();
                stillLoading |= skinRes.is_currently_loading();

                if (skinRes.is_successfully_loaded())
                {
                    skeletonRes = resourceRegistry->get_resource(skinRes->skeleton);

                    if (!skeletonRes)
                    {
                        log::debug("Failed to find skeleton resource for entity {}", entity.value);
                        return false;
                    }

                    skeletonRes.load_start_async();
                    stillLoading |= skeletonRes.is_currently_loading();
                }

                if (!stillLoading && (!skinRes.is_successfully_loaded() || !skeletonRes.is_successfully_loaded()))
                {
                    log::debug("Failed to load skin or skeleton resource for entity {}", entity.value);
                    return false;
                }
            }

            materialRes.load_start_async();
            stillLoading |= materialRes.is_currently_loading();

            // If the mesh is already on GPU we don't care about loading the resource
            auto mesh = drawRegistry.try_get_mesh(meshComponent.mesh);

            if (!mesh)
            {
                meshRes.load_start_async();
                stillLoading |= meshRes.is_currently_loading();
            }

            if (stillLoading)
            {
                deferred.add<mesh_resources>(entity) = {
                    .material = std::move(materialRes),
                    .mesh = std::move(meshRes),
                    .skeleton = std::move(skeletonRes),
                    .skin = std::move(skinRes),
                };

                return false;
            }

            // If loading failed here, we could consider replacing the mesh with something that catches the attention on
            // the error.

            if (!meshRes.is_successfully_loaded() || !materialRes.is_successfully_loaded())
            {
                log::debug("Failed to load mesh resources for entity {}", entity.value);
            }

            if (!mesh)
            {
                mesh = drawRegistry.get_or_create_mesh(meshComponent.mesh);

                if (!mesh)
                {
                    deferred.add<mesh_processed_tag>(entity);
                    return false;
                }
            }

            auto&& [gpuMaterial, pickingId, cachedRefs, gpuMeshComponent] = deferred.add<gpu_material,
                entity_id_component,
                processed_mesh_resources,
                draw_mesh_component,
                draw_raytraced_tag,
                mesh_processed_tag>(entity);

            deferred.remove<mesh_resources>(entity);

            gpuMaterial = convert(*resourceRegistry, *resourceCache, *materialRes);

            pickingId.entityId = entity;

            gpuMeshComponent.mesh = mesh;

            // Store a checksum so we can determine whether or not we want to re-process it
            cachedRefs = processed_mesh_resources::from(meshComponent);

            return true;
        }
    }

    void static_mesh_system::first_update(const ecs::system_update_context& ctx)
    {
        m_drawRegistry = ctx.services->find<draw_registry>();
        OBLO_ASSERT(m_drawRegistry);

        m_resourceRegistry = ctx.services->find<const resource_registry>();
        OBLO_ASSERT(m_resourceRegistry);

        m_resourceCache = ctx.services->find<resource_cache>();
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
            const std::span skinComponents = chunk.try_get<const skin_component>();

            if (skinComponents.empty())
            {
                for (auto&& [e, meshComponent] : chunk.zip<ecs::entity, static_mesh_component>())
                {
                    constexpr bool withSkin = false;

                    try_add_mesh<withSkin>(m_resourceRegistry,
                        m_resourceCache,
                        *m_drawRegistry,
                        e,
                        meshComponent,
                        nullptr,
                        deferred);
                }
            }
            else
            {
                for (auto&& [e, meshComponent, skinComponent] :
                    zip_range(chunk.get<ecs::entity>(), chunk.get<static_mesh_component>(), skinComponents))
                {
                    constexpr bool withSkin = true;

                    const bool meshAdded = try_add_mesh<withSkin>(m_resourceRegistry,
                        m_resourceCache,
                        *m_drawRegistry,
                        e,
                        meshComponent,
                        &skinComponent,
                        deferred);

                    if (!meshAdded)
                    {
                        continue;
                    }

                    const resource_ptr skin = m_resourceRegistry->get_resource(skinComponent.skin);

                    // This shouldn't be doing anything really, since we used it in try_add_mesh
                    OBLO_ASSERT(skin.is_successfully_loaded());
                    skin.load_sync();

                    if (!skin.is_successfully_loaded())
                    {
                        log::error("Failed to load skin on entity {}", e.value);
                        continue;
                    }

                    const resource_ptr skeleton = m_resourceRegistry->get_resource(skin->skeleton);

                    OBLO_ASSERT(skeleton.is_successfully_loaded());
                    skeleton.load_sync();

                    if (!skeleton.is_successfully_loaded())
                    {
                        log::error("Failed to load skeleton on entity {}", e.value);
                        continue;
                    }

                    static_assert(
                        joint_pose_component::joints_per_chunk == joint_skinning_transform_component::joints_per_chunk);

                    constexpr u32 maxJoints = joint_skinning_transform_chunks_component::max_chunks *
                        joint_skinning_transform_component::joints_per_chunk;

                    const u32 numJoints = skin->invBindPoses.size32();

                    if (numJoints > maxJoints)
                    {
                        log::error("Entity {} exceeds the number of joints ({} > {})", e.value, numJoints, maxJoints);
                        continue;
                    }

                    const u32 numChunks = round_up_div(numJoints, joint_skinning_transform_component::joints_per_chunk);

                    joint_skinning_transform_chunks_component& jointChunks =
                        deferred.add<joint_skinning_transform_chunks_component>(e);

                    if (numChunks > 0)
                    {
                        children_component& childrenComponent = ctx.entities->has<children_component>(e)
                            ? ctx.entities->get<children_component>(e)
                            : deferred.add<children_component>(e);

                        childrenComponent.children.reserve(childrenComponent.children.size() + numChunks);

                        for (u32 chunkIndex = 0; chunkIndex < numChunks; ++chunkIndex)
                        {
                            auto&& [chunkEntity, parentComponent, jointTransform, jointPose, drawInstanceId] =
                                deferred.create_with_reserved_id<parent_component,
                                    joint_skinning_transform_component,
                                    joint_pose_component,
                                    // Add draw_instance_id_component to make sure the entity is uploaded to the GPU
                                    draw_instance_id_component,
                                    transient_tag>(*ctx.entities);

                            // This is mostly to make sure we keep track of what was actually filled in by draw_registry
                            drawInstanceId.rtInstanceId = draw_instance_id_component::invalid_id;
                            
                            parentComponent.parent = e;
                            childrenComponent.children.push_back(chunkEntity);

                            jointChunks.chunks[chunkIndex] = chunkEntity;

                            for (u32 globalJointIndex = chunkIndex, localJointIndex = 0;
                                globalJointIndex < chunkIndex + joint_skinning_transform_component::joints_per_chunk &&
                                globalJointIndex < numJoints;
                                ++globalJointIndex, ++localJointIndex)
                            {
                                const auto& jointName = skin->jointNames[globalJointIndex];

                                auto skeletonJointIt = skeleton->jointsHierarchy.begin();

                                // Linear search, could cache the skeleton instead and use a map, not sure if it's worth
                                // persisting the skeletons though
                                for (; skeletonJointIt != skeleton->jointsHierarchy.end(); ++skeletonJointIt)
                                {
                                    if (skeletonJointIt->name == jointName)
                                    {
                                        break;
                                    }
                                }

                                if (skeletonJointIt == skeleton->jointsHierarchy.end())
                                {
                                    log::error("Unable to find joint {} on entity {}", jointName, e.value);
                                }

                                const skeleton::joint& skeletonJoint = *skeletonJointIt;

                                jointPose.currentPoses[localJointIndex] = skeletonJoint.transform;
                                jointPose.defaultPoses[localJointIndex] = skeletonJoint.transform;
                                jointPose.invBindPoses[localJointIndex] = skin->invBindPoses[globalJointIndex];

                                jointTransform.jointMatrices[localJointIndex] =
                                    skin->invBindPoses[globalJointIndex] * skeletonJoint.transform;
                            }
                        }
                    }
                }
            }
        }

        deferred.apply(*ctx.entities);

        // We could decide to delete the mesh_resource after processing, but we need to double-check how often we are
        // updating materials
    }
}