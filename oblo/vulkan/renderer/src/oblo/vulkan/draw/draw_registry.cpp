#include <oblo/vulkan/draw/draw_registry.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/array_size.hpp>
#include <oblo/core/buffered_array.hpp>
#include <oblo/core/data_format.hpp>
#include <oblo/core/flags.hpp>
#include <oblo/core/frame_allocator.hpp>
#include <oblo/core/iterator/zip_range.hpp>
#include <oblo/core/log.hpp>
#include <oblo/ecs/archetype_storage.hpp>
#include <oblo/ecs/component_type_desc.hpp>
#include <oblo/ecs/range.hpp>
#include <oblo/ecs/type_set.hpp>
#include <oblo/ecs/utility/registration.hpp>
#include <oblo/resource/resource_ptr.hpp>
#include <oblo/resource/resource_ref.hpp>
#include <oblo/resource/resource_registry.hpp>
#include <oblo/scene/assets/mesh.hpp>
#include <oblo/scene/components/global_transform_component.hpp>
#include <oblo/trace/profile.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/data/gpu_aabb.hpp>
#include <oblo/vulkan/draw/mesh_table.hpp>
#include <oblo/vulkan/error.hpp>
#include <oblo/vulkan/monotonic_gbu_buffer.hpp>
#include <oblo/vulkan/staging_buffer.hpp>
#include <oblo/vulkan/vulkan_context.hpp>

#include <array>
#include <charconv>

namespace oblo::vk
{
    namespace
    {
        // TODO: Remove the limitation, instead allocate 1 buffer and sub-allocate it with fixed size batches
        constexpr u32 MaxVerticesPerBatch{4 << 20};
        constexpr u32 MaxIndicesPerBatch{4 << 20};
        constexpr u32 MaxMeshesPerBatch{4 << 10};
        constexpr u32 MaxMeshletsPerBatch{MaxMeshesPerBatch};

        constexpr u32 MaxAttributesCount{u32(attribute_kind::enum_max)};
        using buffer_columns = std::array<buffer_column_description, MaxAttributesCount>;

        enum class vertex_attributes : u8
        {
            position,
            normal,
            tangent,
            bitangent,
            uv0,
            enum_max,
        };

        enum class mesh_data_buffers : u8
        {
            mesh_draw_range,
            aabb,
        };

        constexpr vertex_attributes convert_vertex_attribute(attribute_kind attribute)
        {
            switch (attribute)
            {
            case attribute_kind::position:
                return vertex_attributes::position;
            case attribute_kind::normal:
                return vertex_attributes::normal;
            case attribute_kind::uv0:
                return vertex_attributes::uv0;
            case attribute_kind::tangent:
                return vertex_attributes::tangent;
            case attribute_kind::bitangent:
                return vertex_attributes::bitangent;

            default:
                unreachable();
            }
        }

        constexpr h32<draw_mesh> make_mesh_id(const mesh_handle m)
        {
            return std::bit_cast<h32<draw_mesh>>(m);
        }

        struct draw_instance_component
        {
            mesh_handle mesh;
        };

        struct draw_instance_id_component
        {
            u32 rtinstanceId : 24;
        };

        struct mesh_draw_range
        {
            u32 vertexOffset;
            u32 indexOffset;
            u32 meshletOffset;
            u32 meshletCount;
        };

        draw_instance_id_component make_global_instance_id(u32 instanceTableId, u32 instanceIndex)
        {
            constexpr u32 instanceIndexBits = 20;
            constexpr u32 mask = (1u << instanceIndexBits) - 1;

            // We use 24 bits, because that is what the ray tracing pipeline allows for custom ids
            // We reserve 4 for the instance table and 20 for the instance index
            OBLO_ASSERT(instanceTableId < (1u << (24 - instanceIndexBits)));
            OBLO_ASSERT(instanceIndex <= mask);

            return {(instanceIndex & mask) | (instanceTableId << instanceIndexBits)};
        }
    }

    struct draw_registry::blas
    {
        rt_acceleration_structure as;
        resource_ref<mesh> mesh;
    };

    struct draw_registry::pending_mesh_upload
    {
        staging_buffer_span src;
        buffer dst;
    };

    struct draw_registry::instance_data_type_info
    {
        std::string name;
        u32 gpuInstanceBufferId;
    };

    draw_registry::draw_registry() = default;

    draw_registry::~draw_registry() = default;

    void draw_registry::init(vulkan_context& ctx,
        staging_buffer& stagingBuffer,
        string_interner& interner,
        ecs::entity_registry& entities,
        resource_registry& resourceRegistry)
    {
        m_ctx = &ctx;
        m_stagingBuffer = &stagingBuffer;
        m_entities = &entities;
        m_resourceRegistry = &resourceRegistry;

        mesh_attribute_description attributes[u32(vertex_attributes::enum_max)]{};

        attributes[u32(vertex_attributes::position)] = {
            .name = interner.get_or_add("in_Position"),
            .elementSize = sizeof(f32) * 3,
        };

        attributes[u32(vertex_attributes::normal)] = {
            .name = interner.get_or_add("in_Normal"),
            .elementSize = sizeof(f32) * 3,
        };

        attributes[u32(vertex_attributes::tangent)] = {
            .name = interner.get_or_add("in_Tangent"),
            .elementSize = sizeof(f32) * 3,
        };

        attributes[u32(vertex_attributes::bitangent)] = {
            .name = interner.get_or_add("in_Bitangent"),
            .elementSize = sizeof(f32) * 3,
        };

        attributes[u32(vertex_attributes::uv0)] = {
            .name = interner.get_or_add("in_UV0"),
            .elementSize = sizeof(f32) * 2,
        };

        OBLO_ASSERT(
            [&attributes]
            {
                for (auto& a : attributes)
                {
                    if (!a.name)
                    {
                        return false;
                    }
                }

                return true;
            }());

        const mesh_attribute_description meshData[] = {
            {
                .name = interner.get_or_add("b_meshDrawRange"),
                .elementSize = sizeof(mesh_draw_range),
            },
            {
                .name = interner.get_or_add("b_MeshAABBs"),
                .elementSize = sizeof(gpu_aabb),
            },
        };

        static_assert(array_size(meshData) == MeshBuffersCount);

        for (usize i = 0; i < MeshBuffersCount; ++i)
        {
            m_meshDataNames[i] = meshData[i].name;
        }

        [[maybe_unused]] const auto meshDbInit = m_meshes.init({
            .allocator = ctx.get_allocator(),
            .resourceManager = ctx.get_resource_manager(),
            .attributes = attributes,
            .meshData = meshData,
            .vertexBufferUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
            .indexBufferUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
            .meshBufferUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
            .tableVertexCount = MaxVerticesPerBatch,
            .tableIndexCount = MaxIndicesPerBatch,
            .tableMeshCount = MaxMeshesPerBatch,
            .tableMeshletCount = MaxMeshletsPerBatch,
        });

        OBLO_ASSERT(meshDbInit);

        struct mesh_index_none_tag
        {
        };

        struct mesh_index_u8_tag
        {
        };

        struct mesh_index_u16_tag
        {
        };

        struct mesh_index_u32_tag
        {
        };

        m_typeRegistry = &entities.get_type_registry();

        m_typeRegistry->register_component(ecs::make_component_type_desc<draw_mesh_component>());
        m_typeRegistry->register_tag(ecs::make_tag_type_desc<draw_raytraced_tag>());

        m_instanceComponent =
            m_typeRegistry->register_component(ecs::make_component_type_desc<draw_instance_component>());

        m_instanceIdComponent =
            m_typeRegistry->register_component(ecs::make_component_type_desc<draw_instance_id_component>());

        register_instance_data(m_instanceComponent, "i_MeshHandles");

        m_indexNoneTag = m_typeRegistry->register_tag(ecs::make_tag_type_desc<mesh_index_none_tag>());
        m_indexU8Tag = m_typeRegistry->register_tag(ecs::make_tag_type_desc<mesh_index_u8_tag>());
        m_indexU16Tag = m_typeRegistry->register_tag(ecs::make_tag_type_desc<mesh_index_u16_tag>());
        m_indexU32Tag = m_typeRegistry->register_tag(ecs::make_tag_type_desc<mesh_index_u32_tag>());

        m_rtScratchBuffer.init(*m_ctx,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        m_rtInstanceBuffer.init(*m_ctx,
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }

    void draw_registry::shutdown()
    {
        m_meshes.shutdown();
        m_rtScratchBuffer.shutdown();
        m_rtInstanceBuffer.shutdown();

        for (auto& blas : m_meshToBlas.values())
        {
            release(blas.as);
        }

        m_meshToBlas.clear();

        release(m_tlas);
    }

    void draw_registry::register_instance_data(ecs::component_type type, std::string_view name)
    {
        m_instanceDataTypeNames.emplace(type, std::string{name});
        m_instanceDataTypes.add(type);
        m_isInstanceTypeInfoDirty = true;
    }

    bool draw_registry::needs_reloading_instance_data_types() const
    {
        return m_isInstanceTypeInfoDirty;
    }

    void draw_registry::end_frame()
    {
        m_drawData = {};
        m_drawDataCount = 0;
    }

    h32<draw_mesh> draw_registry::get_or_create_mesh(const resource_ref<mesh>& resourceId)
    {
        if (const auto it = m_cachedMeshes.find(resourceId.id); it != m_cachedMeshes.end())
        {
            return it->second;
        }

        const auto anyResource = m_resourceRegistry->get_resource(resourceId.id);
        const auto meshResource = anyResource.as<mesh>();

        if (!meshResource)
        {
            return {};
        }

        mesh convertedMesh;

        const mesh* const meshPtr = meshResource.get();

        const u32 numAttributes = meshPtr->get_attributes_count();

        u32 vertexAttributesCount{0};

        auto indexType = mesh_index_type::none;

        flags<vertex_attributes> attributeFlags;
        attribute_kind meshAttributes[u32(vertex_attributes::enum_max)];
        u32 attributeIds[u32(vertex_attributes::enum_max)];

        for (u32 i = 0; i < numAttributes; ++i)
        {
            const auto& meshAttribute = meshPtr->get_attribute_at(i);

            if (const auto kind = meshAttribute.kind; is_vertex_attribute(kind))
            {
                const auto a = convert_vertex_attribute(kind);
                meshAttributes[vertexAttributesCount] = kind;
                attributeIds[vertexAttributesCount] = u32(a);
                attributeFlags |= a;

                ++vertexAttributesCount;
            }
            else if (kind == attribute_kind::microindices)
            {
                switch (meshAttribute.format)
                {
                case data_format::u8:
                    indexType = mesh_index_type::u8;
                    break;

                default:
                    OBLO_ASSERT(false, "Rasterization only supports u8 micro-indices");
                    break;
                }
            }
        }

        const auto meshHandle = m_meshes.create_mesh(attributeFlags.data(),
            indexType,
            meshPtr->get_vertex_count(),
            meshPtr->get_index_count(),
            meshPtr->get_meshlet_count());

        buffer indexBuffer{};
        buffer meshletsBuffer{};
        buffer vertexBuffers[u32(vertex_attributes::enum_max)];

        buffer meshDataBuffers[MeshBuffersCount]{};

        [[maybe_unused]] const auto fetchedBuffers = m_meshes.fetch_buffers(meshHandle,
            {attributeIds, vertexAttributesCount},
            {vertexBuffers, vertexAttributesCount},
            &indexBuffer,
            m_meshDataNames,
            meshDataBuffers,
            &meshletsBuffer);

        OBLO_ASSERT(fetchedBuffers);

        if (indexBuffer.buffer)
        {
            const auto data = meshPtr->get_attribute(attribute_kind::microindices);
            defer_upload(data, indexBuffer);
        }

        if (meshletsBuffer.buffer)
        {
            const auto data = meshPtr->get_meshlets();
            defer_upload(as_bytes(data), meshletsBuffer);
        }

        for (u32 i = 0; i < vertexAttributesCount; ++i)
        {
            const auto kind = meshAttributes[i];
            const auto data = meshPtr->get_attribute(kind);

            defer_upload(data, vertexBuffers[i]);
        }

        {
            const auto range = m_meshes.get_table_range(meshHandle);

            const mesh_draw_range drawRange{
                .vertexOffset = range.vertexOffset,
                .indexOffset = range.indexOffset,
                .meshletOffset = range.meshletOffset,
                .meshletCount = range.meshletCount,
            };

            defer_upload(std::as_bytes(std::span{&drawRange, 1}),
                meshDataBuffers[u32(mesh_data_buffers::mesh_draw_range)]);
        }

        {
            const auto aabb = meshPtr->get_aabb();
            const gpu_aabb gpuAabb{.min = aabb.min, .max = aabb.max};
            defer_upload(std::as_bytes(std::span{&gpuAabb, 1}), meshDataBuffers[u32(mesh_data_buffers::aabb)]);
        }

        const h32<draw_mesh> globalMeshId{make_mesh_id(meshHandle)};
        m_cachedMeshes.emplace(resourceId.id, globalMeshId);

        // We need to cache the meshlets to build the BLAS on-demand

        const auto [meshIt, ok] = m_meshToBlas.emplace(globalMeshId);
        OBLO_ASSERT(ok);

        const auto srcMeshlets = meshPtr->get_meshlets();
        meshIt->mesh = resourceId;

        return globalMeshId;
    }

    void draw_registry::create_instances()
    {
        // Just gather entities and create after the iteration for now, should do something better after fixing #7
        struct deferred_creation
        {
            ecs::entity entity;
            h32<draw_mesh> mesh;
        };

        dynamic_array<deferred_creation> entitiesToUpdate;

        for (const auto [entities, meshes] :
            m_entities->range<draw_mesh_component>().exclude<draw_instance_component>())
        {
            for (const auto [entity, mesh] : zip_range(entities, meshes))
            {
                entitiesToUpdate.emplace_back(entity, mesh.mesh);
            }
        }

        for (const auto [entity, mesh] : entitiesToUpdate)
        {
            if (!mesh)
            {
                continue;
            }

            ecs::component_and_tag_sets sets{};

            // We add tags for different index types, because we won't be able to draw these meshes together
            switch (m_meshes.get_index_type({mesh.value}))
            {
            case mesh_index_type::none:
                sets.tags.add(m_indexNoneTag);
                break;
            case mesh_index_type::u8:
                sets.tags.add(m_indexU8Tag);
                break;
            case mesh_index_type::u16:
                sets.tags.add(m_indexU16Tag);
                break;
            case mesh_index_type::u32:
                sets.tags.add(m_indexU32Tag);
                break;
            default:
                unreachable();
            }

            sets.components.add(m_instanceComponent);
            sets.components.add(m_instanceIdComponent);

            m_entities->add(entity, sets);

            auto& instance = m_entities->get<draw_instance_component>(entity);
            instance.mesh = mesh_handle{mesh.value};
        }
    }

    void draw_registry::defer_upload(const std::span<const byte> data, const buffer& b)
    {
        // Do we need info on pipeline barriers?
        [[maybe_unused]] const auto result = m_stagingBuffer->stage(data);

        OBLO_ASSERT(result,
            "We need to flush uploads every now and then instead, or let staging buffer take care of it");

        m_pendingMeshUploads.emplace_back(*result, b);
    }

    void draw_registry::release(rt_acceleration_structure& as)
    {
        if (as.accelerationStructure)
        {
            m_ctx->destroy_deferred(as.accelerationStructure, m_ctx->get_submit_index());
        }

        if (as.buffer.buffer)
        {
            m_ctx->destroy_deferred(as.buffer.buffer, m_ctx->get_submit_index());
            m_ctx->destroy_deferred(as.buffer.allocation, m_ctx->get_submit_index());
        }

        as = {};
    }

    void draw_registry::flush_uploads(VkCommandBuffer commandBuffer)
    {
        if (!m_pendingMeshUploads.empty())
        {
            const VkMemoryBarrier2 before{
                .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            };

            const VkDependencyInfo beforeDependencyInfo{
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .memoryBarrierCount = 1u,
                .pMemoryBarriers = &before,
            };

            vkCmdPipelineBarrier2(commandBuffer, &beforeDependencyInfo);

            for (const auto& upload : m_pendingMeshUploads)
            {
                m_stagingBuffer->upload(commandBuffer, upload.src, upload.dst.buffer, upload.dst.offset);
            }

            const VkMemoryBarrier2 after{
                .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
            };

            const VkDependencyInfo afterDependencyInfo{
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .memoryBarrierCount = 1u,
                .pMemoryBarriers = &after,
            };

            vkCmdPipelineBarrier2(commandBuffer, &afterDependencyInfo);

            m_pendingMeshUploads.clear();
        }
    }

    void draw_registry::generate_mesh_database(frame_allocator& allocator)
    {
        m_meshDatabaseData = m_meshes.create_mesh_table_lookup(allocator);
    }

    void draw_registry::generate_draw_calls(frame_allocator& allocator, staging_buffer& stagingBuffer)
    {
        create_instances();

        const std::span archetypes = m_entities->get_archetypes();

        if (archetypes.empty())
        {
            return;
        }

        batch_draw_data* const frameDrawData = allocate_n<batch_draw_data>(allocator, archetypes.size());

        // We will move this forward again on the first iteration
        batch_draw_data* currentDrawBatch = frameDrawData - 1;
        u32 drawBatches{};

        for (const auto archetype : archetypes)
        {
            const ecs::component_and_tag_sets typeSets = ecs::get_component_and_tag_sets(archetype);

            if (!typeSets.components.contains(m_instanceComponent))
            {
                continue;
            }

            const u32 numEntities = ecs::get_entities_count(archetype);

            if (numEntities == 0)
            {
                continue;
            }

            ++currentDrawBatch;

            *currentDrawBatch = {};

            const std::span componentTypes = ecs::get_component_types(archetype);

            currentDrawBatch->instanceTableId = drawBatches;
            ++drawBatches;

            // TODO: Don't blindly update all instance buffers every frame
            // Update instance buffers

            const auto allComponentsCount = componentTypes.size();

            draw_instance_buffers instanceBuffers{
                .instanceBufferIds = allocate_n<u32>(allocator, allComponentsCount),
                .buffersData = allocate_n<staging_buffer_span>(allocator, allComponentsCount),
                .count = 0u,
            };

            const auto dataOffsets = allocate_n_span<u32>(allocator, allComponentsCount);
            const auto componentData = allocate_n_span<std::byte*>(allocator, allComponentsCount);

            for (usize i = 0; i < allComponentsCount; ++i)
            {
                const auto componentType = componentTypes[i];

                if (!m_instanceDataTypes.contains(componentType))
                {
                    continue;
                }

                const auto& typeDesc = m_typeRegistry->get_component_type_desc(componentType);
                const u32 bufferSize = typeDesc.size * numEntities;

                const expected allocation = stagingBuffer.stage_allocate(bufferSize);
                OBLO_ASSERT(allocation);

                auto* const typeInfo = m_instanceDataTypeNames.try_find(componentType);

                instanceBuffers.buffersData[instanceBuffers.count] = *allocation;
                instanceBuffers.instanceBufferIds[instanceBuffers.count] =
                    typeInfo ? typeInfo->gpuInstanceBufferId : ~u32{};

                ++instanceBuffers.count;
            }

            u32 numProcessedEntities{0};

            ecs::for_each_chunk(archetype,
                componentTypes,
                dataOffsets,
                componentData,
                [&](const ecs::entity*, std::span<std::byte*> componentArrays, u32 numEntitiesInChunk)
                {
                    for (usize i = 0, j = 0; i < allComponentsCount; ++i)
                    {
                        if (componentTypes[i] == m_instanceIdComponent)
                        {
                            auto* const instanceIds =
                                start_lifetime_as_array<draw_instance_id_component>(componentArrays[i],
                                    numEntitiesInChunk);

                            const auto instanceTableId = currentDrawBatch->instanceTableId;

                            for (u32 e = 0; e < numEntitiesInChunk; ++e)
                            {
                                const auto instanceIndex = e + numProcessedEntities;
                                instanceIds[e] = make_global_instance_id(instanceTableId, instanceIndex);
                            }

                            continue;
                        }
                        else if (!m_instanceDataTypes.contains(componentTypes[i]))
                        {
                            continue;
                        }

                        const auto& typeDesc = m_typeRegistry->get_component_type_desc(componentTypes[i]);

                        const u32 chunkSize = typeDesc.size * numEntitiesInChunk;
                        const u32 dstOffset = typeDesc.size * numProcessedEntities;

                        const auto& instanceBuffer = instanceBuffers.buffersData[j];

                        stagingBuffer.copy_to(instanceBuffer, dstOffset, {componentArrays[i], chunkSize});

                        ++j;
                    }

                    numProcessedEntities += numEntitiesInChunk;
                });

            currentDrawBatch->instanceBuffers = instanceBuffers;
            currentDrawBatch->numInstances = numProcessedEntities;
        }

        m_drawData = frameDrawData;
        m_drawDataCount = drawBatches;
    }

    void draw_registry::generate_raytracing_structures(frame_allocator& allocator, VkCommandBuffer commandBuffer)
    {
        OBLO_PROFILE_SCOPE();

        const auto vkFn = m_ctx->get_loaded_functions();

        const auto entityRange =
            m_entities->range<draw_mesh_component, draw_instance_id_component, global_transform_component>()
                .with<draw_raytraced_tag>();

        VkAccelerationStructureBuildRangeInfoKHR offset;
        offset.primitiveOffset;

        struct blas_build_info
        {
            std::span<VkAccelerationStructureGeometryKHR> geometry;
            std::span<VkAccelerationStructureBuildRangeInfoKHR> ranges;
            std::span<u32> maxPrimitives;
            VkAccelerationStructureKHR accelerationStructure{};
            VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
            allocated_buffer blasBuffer{};
            VkDeviceSize scratchSize{};
        };

        dynamic_array<VkAccelerationStructureInstanceKHR> instances;
        instances.reserve(4096);

        dynamic_array<blas_build_info> blasBuilds;
        blasBuilds.reserve(256);

        VkDeviceSize blasScratchSize{};

        auto& gpuAllocator = m_ctx->get_allocator();

        const auto firstBlasUpload = m_pendingMeshUploads.size();

        for (auto&& [entities, meshes, drawInstanceIds, transforms] : entityRange)
        {
            for (const auto&& [mesh, drawInstanceId, transform] : zip_range(meshes, drawInstanceIds, transforms))
            {
                auto* const blas = m_meshToBlas.try_find(mesh.mesh);

                if (!blas)
                {
                    continue;
                }

                if (!blas->as.accelerationStructure)
                {
                    const auto meshPtr = m_resourceRegistry->get_resource(blas->mesh.id).as<oblo::mesh>();

                    if (!meshPtr)
                    {
                        continue;
                    }

                    const u32 positionAttribute[] = {u32(vertex_attributes::position)};
                    buffer positionBuffer[1];

                    const mesh_handle meshHandle = std::bit_cast<mesh_handle>(mesh.mesh);

                    if (!m_meshes
                             .fetch_buffers(meshHandle, positionAttribute, positionBuffer, nullptr, {}, {}, nullptr))
                    {
                        continue;
                    }

                    const VkDeviceAddress vertexAddress =
                        m_ctx->get_device_address(positionBuffer[0].buffer) + positionBuffer[0].offset;

                    // TODO: We need a better solution, we can't use the microindices (mostly because of no support of
                    // building the BLAS from u8 indices)
                    allocated_buffer allocatedIndexBuffer;

                    const auto indexType = meshPtr->get_attribute_format(attribute_kind::indices);
                    const auto indexData = meshPtr->get_attribute(attribute_kind::indices);

                    VkIndexType vkIndexType;

                    switch (indexType)
                    {
                    case data_format::u16:
                        vkIndexType = VK_INDEX_TYPE_UINT16;
                        break;
                    case data_format::u32:
                        vkIndexType = VK_INDEX_TYPE_UINT32;
                        break;
                    default:
                        OBLO_ASSERT(false);
                        continue;
                    }

                    const auto indexBufferByteSize = narrow_cast<u32>(indexData.size_bytes());

                    if (gpuAllocator.create_buffer(
                            {
                                .size = indexBufferByteSize,
                                .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                                .memoryUsage = memory_usage::gpu_only,
                            },
                            &allocatedIndexBuffer) != VK_SUCCESS)
                    {
                        continue;
                    }

                    // We need this buffer for one frame only, destroy it after
                    const auto submitIndex = m_ctx->get_submit_index();

                    m_ctx->destroy_deferred(allocatedIndexBuffer.buffer, submitIndex);
                    m_ctx->destroy_deferred(allocatedIndexBuffer.allocation, submitIndex);

                    const buffer indexBuffer{
                        .buffer = allocatedIndexBuffer.buffer,
                        .size = indexBufferByteSize,
                    };

                    const VkDeviceAddress indexAddress = m_ctx->get_device_address(indexBuffer.buffer);

                    const auto geometry = allocate_n_span<VkAccelerationStructureGeometryKHR>(allocator, 1u);
                    const auto ranges = allocate_n_span<VkAccelerationStructureBuildRangeInfoKHR>(allocator, 1u);
                    const auto maxPrimitives = allocate_n_span<u32>(allocator, 1u);

                    {
                        VkAccelerationStructureGeometryKHR& g = geometry[0];
                        VkAccelerationStructureBuildRangeInfoKHR& r = ranges[0];

                        g = {
                            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
                            .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
                            .geometry =
                                {
                                    .triangles =
                                        {
                                            .sType =
                                                VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
                                            .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
                                            .vertexData = {.deviceAddress = vertexAddress},
                                            .vertexStride = sizeof(vec3),
                                            .maxVertex = meshPtr->get_vertex_count() - 1,
                                            .indexType = vkIndexType,
                                            .indexData = {.deviceAddress = indexAddress},
                                        },
                                },
                            .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
                        };

                        r = {
                            .primitiveCount = meshPtr->get_index_count() / 3,
                        };

                        maxPrimitives[0] = r.primitiveCount;
                    }

                    const VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
                        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
                        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
                        .flags = 0, // Maybe allow compaction
                        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
                        .geometryCount = u32(geometry.size()),
                        .pGeometries = geometry.data(),
                    };

                    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
                        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
                    };

                    vkFn.vkGetAccelerationStructureBuildSizesKHR(m_ctx->get_device(),
                        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                        &buildInfo,
                        maxPrimitives.data(),
                        &sizeInfo);

                    // TODO: We should sub-allocate a buffer instead, but good enough for now
                    allocated_buffer blasBuffer{};

                    gpuAllocator.create_buffer(
                        {
                            .size = narrow_cast<u32>(sizeInfo.accelerationStructureSize),
                            .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                            .memoryUsage = memory_usage::gpu_only,
                        },
                        &blasBuffer);

                    const VkAccelerationStructureCreateInfoKHR createInfo{
                        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
                        .buffer = blasBuffer.buffer,
                        .size = sizeInfo.accelerationStructureSize,
                        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
                    };

                    if (vkFn.vkCreateAccelerationStructureKHR(m_ctx->get_device(),
                            &createInfo,
                            gpuAllocator.get_allocation_callbacks(),
                            &blas->as.accelerationStructure) != VK_SUCCESS)
                    {
                        log::error("Failed to create blas for mesh {}", mesh.mesh.value);
                        gpuAllocator.destroy(blasBuffer);
                        continue;
                    }

                    const VkAccelerationStructureDeviceAddressInfoKHR asAddrInfo{
                        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
                        .accelerationStructure = blas->as.accelerationStructure,
                    };

                    blas->as.buffer = blasBuffer;

                    blas->as.deviceAddress =
                        vkFn.vkGetAccelerationStructureDeviceAddressKHR(m_ctx->get_device(), &asAddrInfo);

                    auto& blasBuild = blasBuilds.emplace_back();

                    blasBuild.geometry = geometry;
                    blasBuild.ranges = ranges;
                    blasBuild.accelerationStructure = blas->as.accelerationStructure;
                    blasBuild.buildInfo = buildInfo;
                    blasBuild.blasBuffer = blasBuffer;
                    blasBuild.scratchSize = round_up_multiple<VkDeviceSize>(sizeInfo.buildScratchSize, 256);

                    blasScratchSize += blasBuild.scratchSize;

                    defer_upload(indexData, indexBuffer);
                }

                VkTransformMatrixKHR vkTransform;

                for (u32 i = 0; i < 3; ++i)
                {
                    for (u32 j = 0; j < 4; ++j)
                    {
                        vkTransform.matrix[i][j] = transform.localToWorld.columns[j][i];
                    }
                }

                // Add the instance to the TLAS

                instances.push_back({
                    .transform = vkTransform,
                    .instanceCustomIndex = drawInstanceId.rtinstanceId,
                    .mask = 0xff,
                    .instanceShaderBindingTableRecordOffset = 0,
                    .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
                    .accelerationStructureReference = blas->as.deviceAddress,
                });
            }
        }

        // Actually build the queued BLAS

        if (const usize numQueuedBuilds = blasBuilds.size())
        {
            if (firstBlasUpload != m_pendingMeshUploads.size())
            {
                const auto n = m_pendingMeshUploads.size() - firstBlasUpload;
                // Add barriers for the index buffer upload to build the BLAS
                const auto barriers = allocate_n_span<VkBufferMemoryBarrier2>(allocator, n);

                for (auto i = 0; i < n; ++i)
                {
                    auto& buf = m_pendingMeshUploads[firstBlasUpload + i].dst;

                    barriers[i] = {
                        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                        .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                        .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                        .dstStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                        .dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
                        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .buffer = buf.buffer,
                        .offset = buf.offset,
                        .size = buf.size,
                    };
                }

                // Flush any index buffer upload
                flush_uploads(commandBuffer);

                const VkDependencyInfo dependencyInfo{
                    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                    .bufferMemoryBarrierCount = u32(n),
                    .pBufferMemoryBarriers = barriers.data(),
                };

                vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
            }

            const auto geometryInfo =
                allocate_n_span<VkAccelerationStructureBuildGeometryInfoKHR>(allocator, numQueuedBuilds);

            const auto buildRangeInfo =
                allocate_n_span<VkAccelerationStructureBuildRangeInfoKHR*>(allocator, numQueuedBuilds);

            allocated_buffer scratchBuffer{};

            OBLO_VK_PANIC(gpuAllocator.create_buffer(
                {
                    .size = narrow_cast<u32>(blasScratchSize),
                    .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                    .memoryUsage = memory_usage::gpu_only,
                },
                &scratchBuffer));

            m_ctx->destroy_deferred(scratchBuffer.buffer, m_ctx->get_submit_index());
            m_ctx->destroy_deferred(scratchBuffer.allocation, m_ctx->get_submit_index());

            auto scratchAddress = m_ctx->get_device_address(scratchBuffer.buffer);

            for (usize i = 0; i < numQueuedBuilds; ++i)
            {
                auto& blasBuild = blasBuilds[i];

                geometryInfo[i] = blasBuild.buildInfo;
                geometryInfo[i].dstAccelerationStructure = blasBuild.accelerationStructure;
                geometryInfo[i].scratchData.deviceAddress = scratchAddress;
                buildRangeInfo[i] = blasBuild.ranges.data();

                scratchAddress += blasBuild.scratchSize;
            }

            vkFn.vkCmdBuildAccelerationStructuresKHR(commandBuffer,
                u32(blasBuilds.size()),
                geometryInfo.data(),
                buildRangeInfo.data());

            // Add a barrier between BLAS and TLAS construction

            const VkMemoryBarrier2 tlasBarrier{
                .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                .srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
                .dstStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                .dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR,
            };

            const VkDependencyInfo dependencyInfo{
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .memoryBarrierCount = 1u,
                .pMemoryBarriers = &tlasBarrier,
            };

            vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
        }

        // Temporarily we just destroy the TLAS every frame and recreate it

        if (m_tlas.accelerationStructure)
        {
            m_ctx->destroy_deferred(m_tlas.accelerationStructure, m_ctx->get_submit_index());
            m_ctx->destroy_deferred(m_tlas.buffer.buffer, m_ctx->get_submit_index());
            m_ctx->destroy_deferred(m_tlas.buffer.allocation, m_ctx->get_submit_index());

            m_tlas = {};
        }

        // Finally build the TLAS
        VkDeviceAddress instanceBufferDeviceAddress{};

        if (!instances.empty())
        {
            m_rtInstanceBuffer.resize_discard(u32(instances.size_bytes()));
            const auto instanceBuffer = m_rtInstanceBuffer.get_buffer();

            void* instanceBufferPtr;
            OBLO_VK_PANIC(gpuAllocator.map(instanceBuffer.allocation, &instanceBufferPtr));
            std::memcpy(instanceBufferPtr, instances.data(), instances.size_bytes());

            gpuAllocator.invalidate_mapped_memory_ranges({&instanceBuffer.allocation, 1});
            gpuAllocator.unmap(instanceBuffer.allocation);

            instanceBufferDeviceAddress = m_ctx->get_device_address(instanceBuffer);
        }

        const VkAccelerationStructureGeometryKHR accelerationStructureGeometry{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
            .geometry =
                {
                    .instances =
                        {
                            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
                            .arrayOfPointers = VK_FALSE,
                            .data = {.deviceAddress = instanceBufferDeviceAddress},
                        },
                },
            .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
        };

        const VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
            .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
            .geometryCount = 1u,
            .pGeometries = &accelerationStructureGeometry,
        };

        const u32 instanceCount = u32(instances.size());

        VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
        };

        vkFn.vkGetAccelerationStructureBuildSizesKHR(m_ctx->get_device(),
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &accelerationStructureBuildGeometryInfo,
            &instanceCount,
            &sizeInfo);

        gpuAllocator.create_buffer(
            {
                .size = narrow_cast<u32>(sizeInfo.accelerationStructureSize),
                .usage =
                    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                .memoryUsage = memory_usage::gpu_only,
            },
            &m_tlas.buffer);

        const VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .buffer = m_tlas.buffer.buffer,
            .size = sizeInfo.accelerationStructureSize,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        };

        vkFn.vkCreateAccelerationStructureKHR(m_ctx->get_device(),
            &accelerationStructureCreateInfo,
            gpuAllocator.get_allocation_callbacks(),
            &m_tlas.accelerationStructure);

        const VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .accelerationStructure = m_tlas.accelerationStructure,
        };

        m_tlas.deviceAddress =
            vkFn.vkGetAccelerationStructureDeviceAddressKHR(m_ctx->get_device(), &accelerationDeviceAddressInfo);

        if (!instances.empty())
        {
            m_rtScratchBuffer.resize_discard(narrow_cast<u32>(sizeInfo.buildScratchSize));
            const auto tlasScratchBuffer = m_rtScratchBuffer.get_buffer();

            const VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo{
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
                .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
                .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
                .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
                .dstAccelerationStructure = m_tlas.accelerationStructure,
                .geometryCount = 1,
                .pGeometries = &accelerationStructureGeometry,
                .scratchData =
                    {
                        .deviceAddress = m_ctx->get_device_address(tlasScratchBuffer),
                    },
            };

            const VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{
                .primitiveCount = instanceCount,
                .primitiveOffset = 0,
                .firstVertex = 0,
                .transformOffset = 0,
            };

            const VkAccelerationStructureBuildRangeInfoKHR* const accelerationStructureBuildRangeInfos[] = {
                &accelerationStructureBuildRangeInfo,
            };

            vkFn.vkCmdBuildAccelerationStructuresKHR(commandBuffer,
                1,
                &accelerationBuildGeometryInfo,
                accelerationStructureBuildRangeInfos);
        }
    }

    std::string_view draw_registry::refresh_instance_data_defines(frame_allocator& allocator)
    {
        if (m_instanceDataTypeNames.empty())
        {
            return {};
        }

        constexpr auto sizePerLine = sizeof("#define OBLO_INSTANCE_DATA_ 99\n") + 128;

        auto* const str = reinterpret_cast<char*>(allocator.allocate(m_instanceDataTypeNames.size() * sizePerLine, 1));
        auto* it = str;

        u32 id{};

        for (auto& info : m_instanceDataTypeNames.values())
        {
            const auto newId = id++;
            info.gpuInstanceBufferId = newId;

            it = std::format_to(it, "#define OBLO_INSTANCE_DATA_{} {}\n", info.name, newId);
        }

        return {str, it};
    }

    std::span<const batch_draw_data> draw_registry::get_draw_calls() const
    {
        return {m_drawData, m_drawDataCount};
    }

    std::span<const std::byte> draw_registry::get_mesh_database_data() const
    {
        return m_meshDatabaseData;
    }

    const VkAccelerationStructureKHR draw_registry::get_tlas() const
    {
        return m_tlas.accelerationStructure;
    }

    void draw_registry::debug_log(const batch_draw_data& drawData) const
    {
        char stringBuffer[4096];

        auto fmtAppend = [outIt = stringBuffer, &stringBuffer]<typename... T>(const std::format_string<T...>& fmt,
                             T&&... args) mutable
        {
            outIt = std::format_to(outIt, fmt, std::forward<T>(args)...);
            *outIt = '\0';
            OBLO_ASSERT(outIt < std::end(stringBuffer));
        };

        fmtAppend("Entities count: {}\n", drawData.numInstances);

        fmtAppend("Listing {} instance buffers: \n", drawData.instanceBuffers.count);

        for (u32 i = 0; i < drawData.instanceBuffers.count; ++i)
        {
            const auto id = drawData.instanceBuffers.instanceBufferIds[i];
            const auto buffer = drawData.instanceBuffers.buffersData[i];

            const auto bufferSize = (buffer.segments[0].end - buffer.segments[0].begin) +
                (buffer.segments[1].end - buffer.segments[1].begin);

            const auto name = m_instanceDataTypeNames.values()[id].name;

            fmtAppend("{} [id: {}] [size: {}]\n", name, id, bufferSize);
        }

        log::debug("{}", stringBuffer);
    }
}