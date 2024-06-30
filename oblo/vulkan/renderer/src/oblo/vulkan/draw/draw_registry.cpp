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
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/data/gpu_aabb.hpp>
#include <oblo/vulkan/draw/mesh_table.hpp>
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

        struct mesh_draw_range
        {
            u32 vertexOffset;
            u32 indexOffset;
            u32 meshletOffset;
            u32 meshletCount;
        };
    }

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

    void draw_registry::init(
        vulkan_context& ctx, staging_buffer& stagingBuffer, string_interner& interner, ecs::entity_registry& entities)
    {
        m_ctx = &ctx;
        m_stagingBuffer = &stagingBuffer;
        m_entities = &entities;

        mesh_attribute_description attributes[u32(vertex_attributes::enum_max)];

        attributes[u32(vertex_attributes::position)] = {
            .name = interner.get_or_add("in_Position"),
            .elementSize = sizeof(f32) * 3,
        };

        attributes[u32(vertex_attributes::normal)] = {
            .name = interner.get_or_add("in_Normal"),
            .elementSize = sizeof(f32) * 3,
        };

        attributes[u32(vertex_attributes::uv0)] = {
            .name = interner.get_or_add("in_UV0"),
            .elementSize = sizeof(f32) * 2,
        };

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
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            .indexBufferUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            .meshBufferUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
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

        m_instanceComponent =
            m_typeRegistry->register_component(ecs::make_component_type_desc<draw_instance_component>());

        register_instance_data(m_instanceComponent, "i_MeshHandles");

        m_indexNoneTag = m_typeRegistry->register_tag(ecs::make_tag_type_desc<mesh_index_none_tag>());
        m_indexU8Tag = m_typeRegistry->register_tag(ecs::make_tag_type_desc<mesh_index_u8_tag>());
        m_indexU16Tag = m_typeRegistry->register_tag(ecs::make_tag_type_desc<mesh_index_u16_tag>());
        m_indexU32Tag = m_typeRegistry->register_tag(ecs::make_tag_type_desc<mesh_index_u32_tag>());
    }

    void draw_registry::shutdown()
    {
        m_meshes.shutdown();
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

    h32<draw_mesh> draw_registry::get_or_create_mesh(oblo::resource_registry& resourceRegistry,
        const resource_ref<mesh>& resourceId)
    {
        if (const auto it = m_cachedMeshes.find(resourceId.id); it != m_cachedMeshes.end())
        {
            return it->second;
        }

        const auto anyResource = resourceRegistry.get_resource(resourceId.id);
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

            if (const auto kind = meshAttribute.kind; kind != attribute_kind::indices)
            {
                const auto a = convert_vertex_attribute(kind);
                meshAttributes[vertexAttributesCount] = kind;
                attributeIds[vertexAttributesCount] = u32(a);
                attributeFlags |= a;

                ++vertexAttributesCount;
            }
            else
            {
                switch (meshAttribute.format)
                {
                case data_format::u8:
                    indexType = mesh_index_type::u8;
                    break;

                case data_format::u16:
                    indexType = mesh_index_type::u16;
                    break;

                case data_format::u32:
                    indexType = mesh_index_type::u32;
                    break;

                default:
                    OBLO_ASSERT(false, "Unhandled index format");
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

        const auto doUpload = [this](const std::span<const std::byte> data, const buffer& b)
        {
            // Do we need info on pipeline barriers?
            [[maybe_unused]] const auto result = m_stagingBuffer->stage(data);

            OBLO_ASSERT(result,
                "We need to flush uploads every now and then instead, or let staging buffer take care of it");

            m_pendingMeshUploads.emplace_back(*result, b);
        };

        if (indexBuffer.buffer)
        {
            const auto data = meshPtr->get_attribute(attribute_kind::indices);
            doUpload(data, indexBuffer);
        }

        if (meshletsBuffer.buffer)
        {
            const auto data = meshPtr->get_meshlets();
            doUpload(as_bytes(data), meshletsBuffer);
        }

        for (u32 i = 0; i < vertexAttributesCount; ++i)
        {
            const auto kind = meshAttributes[i];
            const auto data = meshPtr->get_attribute(kind);

            doUpload(data, vertexBuffers[i]);
        }

        {
            const auto range = m_meshes.get_table_range(meshHandle);

            const mesh_draw_range drawRange{
                .vertexOffset = range.vertexOffset,
                .indexOffset = range.indexOffset,
                .meshletOffset = range.meshletOffset,
                .meshletCount = range.meshletCount,
            };

            doUpload(std::as_bytes(std::span{&drawRange, 1}), meshDataBuffers[u32(mesh_data_buffers::mesh_draw_range)]);
        }

        {
            const auto aabb = meshPtr->get_aabb();
            const gpu_aabb gpuAabb{.min = aabb.min, .max = aabb.max};
            doUpload(std::as_bytes(std::span{&gpuAabb, 1}), meshDataBuffers[u32(mesh_data_buffers::aabb)]);
        }

        const h32<draw_mesh> globalMeshId{make_mesh_id(meshHandle)};
        m_cachedMeshes.emplace(resourceId.id, globalMeshId);

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

            m_entities->add(entity, sets);

            auto& instance = m_entities->get<draw_instance_component>(entity);
            instance.mesh = mesh_handle{mesh.value};
        }
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
                        if (!m_instanceDataTypes.contains(componentTypes[i]))
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