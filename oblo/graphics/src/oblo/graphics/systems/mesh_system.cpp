#include <oblo/graphics/systems/mesh_system.hpp>

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
#include <oblo/graphics/components/animation_component.hpp>
#include <oblo/graphics/components/gpu_components.hpp>
#include <oblo/graphics/components/mesh_internal.hpp>
#include <oblo/graphics/components/skin_component.hpp>
#include <oblo/graphics/components/static_mesh_component.hpp>
#include <oblo/log/log.hpp>
#include <oblo/math/transform.hpp>
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

    struct mesh_system::skin_info
    {
        resource_ptr<skin> skin;
        resource_ptr<skeleton> skeleton;
        bool isFullyInitialized;

        std::unordered_map<string_view, skeleton_joint_index_t, hash<string_view>> jointNameToIndex;
    };

    mesh_system::mesh_system() = default;

    mesh_system::~mesh_system() = default;

    void mesh_system::first_update(const ecs::system_update_context& ctx)
    {
        m_drawRegistry = ctx.services->find<draw_registry>();
        OBLO_ASSERT(m_drawRegistry);

        m_resourceRegistry = ctx.services->find<const resource_registry>();
        OBLO_ASSERT(m_resourceRegistry);

        m_resourceCache = ctx.services->find<resource_cache>();
        OBLO_ASSERT(m_resourceCache);

        update(ctx);
    }

    void mesh_system::update(const ecs::system_update_context& ctx)
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

        dynamic_array<skeleton_joint_index_t> skeletonToSkinMapping{ctx.frameAllocator};

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
                    const skin_info* const skinInfo = get_or_add_skin(skinComponent.skin);

                    if (!skinInfo)
                    {
                        continue;
                    }

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

                    static_assert(
                        joint_pose_component::joints_per_chunk == joint_skinning_transform_component::joints_per_chunk);

                    constexpr u32 maxJoints = joint_skinning_transform_chunks_component::max_chunks *
                        joint_skinning_transform_component::joints_per_chunk;

                    const resource_ptr<skin>& skin = skinInfo->skin;
                    const resource_ptr<skeleton>& skeleton = skinInfo->skeleton;

                    const u32 numJoints = skin->invBindPoses.size32();

                    if (numJoints > maxJoints)
                    {
                        log::error("Entity {} exceeds the number of joints ({} > {})", e.value, numJoints, maxJoints);
                        continue;
                    }

                    const u32 numChunks = round_up_div(numJoints, joint_skinning_transform_component::joints_per_chunk);

                    joint_skinning_transform_chunks_component& jointChunks =
                        deferred.add<joint_skinning_transform_chunks_component>(e);

                    jointChunks.numJoints = numJoints;

                    if (numChunks > 0)
                    {
                        children_component& childrenComponent = ctx.entities->has<children_component>(e)
                            ? ctx.entities->get<children_component>(e)
                            : deferred.add<children_component>(e);

                        childrenComponent.children.reserve(childrenComponent.children.size() + numChunks);

                        skeletonToSkinMapping.clear();
                        skeletonToSkinMapping.reserve(maxJoints);
                        skeletonToSkinMapping.resize(skeleton->jointsHierarchy.size(), skeleton::joint::no_parent);

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

                            for (u32 skinJointIndex = chunkIndex * joint_pose_component::joints_per_chunk,
                                     localJointIndex = 0;
                                localJointIndex < joint_skinning_transform_component::joints_per_chunk &&
                                skinJointIndex < numJoints;
                                ++skinJointIndex, ++localJointIndex)
                            {
                                const auto& jointName = skin->jointNames[skinJointIndex];

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

                                const isize skeletonJointIndex = skeletonJointIt - skeleton->jointsHierarchy.begin();
                                skeletonToSkinMapping[skeletonJointIndex] =
                                    narrow_cast<skeleton_joint_index_t>(skinJointIndex);

                                const skeleton::joint& skeletonJoint = *skeletonJointIt;

                                jointPose.localPoses[localJointIndex] = {
                                    skeletonJoint.translation,
                                    skeletonJoint.rotation,
                                    skeletonJoint.scale,
                                };

                                jointPose.invBindPoses[localJointIndex] = skin->invBindPoses[skinJointIndex];

                                const skeleton_joint_index_t parentSkinJointIndex =
                                    skeletonJoint.parentIndex != skeleton::joint::no_parent
                                    ? skeletonToSkinMapping[skeletonJoint.parentIndex]
                                    : skeleton::joint::no_parent;

                                jointPose.parentJointIndices[localJointIndex] = parentSkinJointIndex;
                            }
                        }
                    }
                }
            }
        }

        deferred.apply(*ctx.entities);

        // Apply the animations
        for (auto&& chunk : ctx.entities->range<const skin_component,
                 const joint_skinning_transform_chunks_component,
                 const animation_progress_component>())
        {
            for (auto&& [e, skinComponent, jointChunk, progress] : chunk.zip<ecs::entity,
                     skin_component,
                     joint_skinning_transform_chunks_component,
                     animation_progress_component>())
            {
                auto* const skinInfo = get_or_add_skin(skinComponent.skin);

                if (!skinInfo)
                {
                    continue;
                }

                for (auto&& jointAnimation : progress.jointAnimations)
                {
                    const string_view jointName = jointAnimation.jointName;

                    const auto jointIt = skinInfo->jointNameToIndex.find(jointName);

                    if (jointIt == skinInfo->jointNameToIndex.end())
                    {
                        continue;
                    }

                    const usize jointChunkIdx = jointIt->second / joint_pose_component::joints_per_chunk;
                    const usize jointLocalIdx = jointIt->second % joint_pose_component::joints_per_chunk;

                    if (jointChunkIdx >= joint_skinning_transform_chunks_component::max_chunks ||
                        !jointChunk.chunks[jointChunkIdx])
                    {
                        continue;
                    }

                    joint_pose_component* const pose =
                        ctx.entities->try_get<joint_pose_component>(jointChunk.chunks[jointChunkIdx]);

                    if (!pose)
                    {
                        continue;
                    }

                    switch (jointAnimation.target)
                    {
                    case animation_progress_component::joint_animation::property::translation:
                        pose->localPoses[jointLocalIdx].translation = jointAnimation.translation;
                        break;

                    case animation_progress_component::joint_animation::property::rotation:
                        pose->localPoses[jointLocalIdx].rotation = jointAnimation.rotation;
                        break;

                    case animation_progress_component::joint_animation::property::scale:
                        pose->localPoses[jointLocalIdx].scale = jointAnimation.scale;
                        break;
                    }
                }
            }
        }

        constexpr u32 maxJoints = joint_skinning_transform_chunks_component::max_chunks *
            joint_skinning_transform_component::joints_per_chunk;

        dynamic_array<mat4> jointTransforms{ctx.frameAllocator};
        jointTransforms.resize_default(maxJoints);

        for (auto&& chunk : ctx.entities->range<joint_skinning_transform_chunks_component,
                 const global_transform_component,
                 const skin_component>())
        {
            for (auto&& [e, jointChunk, transformComp, skin] : chunk.zip<ecs::entity,
                     joint_skinning_transform_chunks_component,
                     global_transform_component,
                     skin_component>())
            {
                const auto skinInfoIt = m_skinInfo.find(skin.skin.id);

                u32 jointIndex = 0;

                for (const ecs::entity child : jointChunk.chunks)
                {
                    if (!child)
                    {
                        break;
                    }

                    if (!ctx.entities->has<joint_skinning_transform_component, joint_pose_component>(child))
                    {
                        log::error("Failed to find joint components on entity {}", child.value);
                        break;
                    }

                    auto&& [jointTransform, jointPose] =
                        ctx.entities->get<joint_skinning_transform_component, joint_pose_component>(child);

                    for (u32 localJointIndex = 0; localJointIndex < joint_pose_component::joints_per_chunk;
                        ++localJointIndex, ++jointIndex)
                    {
                        const auto& pose = jointPose.localPoses[localJointIndex];

                        mat4 jointTransformMatrix = make_transform_matrix(pose.translation, pose.rotation, pose.scale);

                        if (const skeleton_joint_index_t parentIndex = jointPose.parentJointIndices[localJointIndex];
                            parentIndex != skeleton::joint::no_parent)
                        {
                            jointTransformMatrix = jointTransforms[parentIndex] * jointTransformMatrix;
                        }
                        else
                        {
                            jointTransformMatrix = transformComp.localToWorld * jointTransformMatrix;
                        }

                        jointTransforms[jointIndex] = jointTransformMatrix;

                        jointTransform.jointMatrices[localJointIndex] =
                            jointTransformMatrix * jointPose.invBindPoses[localJointIndex];
                    }
                }
            }
        }

        // We could decide to delete the mesh_resource after processing, but we need to double-check how often we are
        // updating materials
    }

    const mesh_system::skin_info* mesh_system::get_or_add_skin(resource_ref<skin> skin)
    {
        auto& entry = m_skinInfo[skin.id];

        if (entry.isFullyInitialized)
        {
            return &entry;
        }

        if (!entry.skin)
        {
            entry.skin = m_resourceRegistry->get_resource(skin);
            entry.skin.load_start_async();
        }

        if (entry.skin.is_currently_loading())
        {
            return nullptr;
        }

        if (!entry.skin.is_successfully_loaded())
        {
            log::error("Failed to load skin");
            return nullptr;
        }

        if (!entry.skeleton)
        {
            entry.skeleton = m_resourceRegistry->get_resource(entry.skin->skeleton);
            entry.skeleton.load_start_async();
        }

        if (entry.skeleton.is_currently_loading())
        {
            return nullptr;
        }

        if (!entry.skeleton.is_successfully_loaded())
        {
            log::error("Failed to load skeleton");
            return nullptr;
        }

        skeleton_joint_index_t jointIndex = 0;

        for (const string_view jointName : entry.skin->jointNames)
        {
            // It's ok to cache these here since resources are immutable and they don't move in memory as long as we
            // hold the resource_ptr
            entry.jointNameToIndex[jointName] = jointIndex;
            ++jointIndex;
        }

        entry.isFullyInitialized = true;

        return &entry;
    }
}