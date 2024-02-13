#include <oblo/vulkan/draw/draw_registry.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/data_format.hpp>
#include <oblo/core/flags.hpp>
#include <oblo/core/zip_range.hpp>
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

        constexpr u32 MaxAttributesCount{u32(attribute_kind::enum_max)};
        using buffer_columns = std::array<buffer_column_description, MaxAttributesCount>;

        enum class vertex_attributes : u8
        {
            position,
            normal,
            uv0,
            enum_max,
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
    }

    draw_registry::draw_registry() = default;

    draw_registry::~draw_registry() = default;

    void draw_registry::init(vulkan_context& ctx, staging_buffer& stagingBuffer, string_interner& interner)
    {
        m_ctx = &ctx;
        m_stagingBuffer = &stagingBuffer;
        m_interner = &interner;

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

        [[maybe_unused]] const auto meshDbInit = m_meshes.init({
            .allocator = ctx.get_allocator(),
            .resourceManager = ctx.get_resource_manager(),
            .attributes = attributes,
            .bufferUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            .tableVertexCount = MaxVerticesPerBatch,
            .tableIndexCount = MaxIndicesPerBatch,
        });

        OBLO_ASSERT(meshDbInit);

        m_instances.init(&m_typeRegistry);

        struct mesh_index_none_tag
        {
        };

        struct mesh_index_u16_tag
        {
        };

        struct mesh_index_u32_tag
        {
        };

        const auto meshHandleInstancBuffer = get_or_register({
            .name = "i_MeshHandles",
            .elementSize = sizeof(mesh_handle),
            .elementAlignment = alignof(mesh_handle),
        });

        m_meshComponent = ecs::component_type{meshHandleInstancBuffer.value};
        m_indexNoneTag = m_typeRegistry.register_tag(ecs::make_tag_type_desc<mesh_index_none_tag>());
        m_indexU16Tag = m_typeRegistry.register_tag(ecs::make_tag_type_desc<mesh_index_u16_tag>());
        m_indexU32Tag = m_typeRegistry.register_tag(ecs::make_tag_type_desc<mesh_index_u32_tag>());

        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(m_ctx->get_physical_device(), &properties);

        u32 bufferChunkSize{1u << 26};

        m_storageBuffer.init(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            memory_usage::gpu_only,
            narrow_cast<u8>(properties.limits.minStorageBufferOffsetAlignment),
            bufferChunkSize);

        m_drawCallsBuffer.init(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
            memory_usage::gpu_only,
            alignof(VkDrawIndexedIndirectCommand),
            bufferChunkSize);
    }

    void draw_registry::shutdown()
    {
        m_meshes.shutdown();
        m_storageBuffer.shutdown(*m_ctx);
        m_drawCallsBuffer.shutdown(*m_ctx);
    }

    void draw_registry::end_frame()
    {
        m_drawData = {};
        m_drawDataCount = 0;

        m_meshTablesBuffer = {};
        m_meshTablesBufferOffset = 0;
        m_meshTablesBufferSize = 0;

        m_storageBuffer.restore_all();
        m_drawCallsBuffer.restore_all();
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

        auto* const meshPtr = meshResource.get();

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
            meshPtr->get_index_count());

        buffer indexBuffer{};
        buffer vertexBuffers[u32(vertex_attributes::enum_max)];

        [[maybe_unused]] const auto fetchedBuffers = m_meshes.fetch_buffers(meshHandle,
            {attributeIds, vertexAttributesCount},
            {vertexBuffers, vertexAttributesCount},
            &indexBuffer);

        OBLO_ASSERT(fetchedBuffers);

        const auto doUpload = [this](const std::span<const std::byte> data, const buffer& b)
        {
            [[maybe_unused]] const auto result = m_stagingBuffer->upload(data, b.buffer, b.offset);

            OBLO_ASSERT(result,
                "We need to flush uploads every now and then instead, or let staging buffer take care of it");
        };

        if (indexBuffer.buffer)
        {
            const auto data = meshPtr->get_attribute(attribute_kind::indices);
            doUpload(data, indexBuffer);
        }

        for (u32 i = 0; i < vertexAttributesCount; ++i)
        {
            const auto kind = meshAttributes[i];
            const auto data = meshPtr->get_attribute(kind);

            doUpload(data, vertexBuffers[i]);
        }

        const h32<draw_mesh> globalMeshId{make_mesh_id(meshHandle)};
        m_cachedMeshes.emplace(resourceId.id, globalMeshId);

        return globalMeshId;
    }

    h32<draw_instance> draw_registry::create_instance(
        h32<draw_mesh> mesh, std::span<const h32<draw_buffer>> buffers, std::span<std::byte*> outData)
    {
        OBLO_ASSERT(buffers.size() == outData.size());

        ecs::component_and_tag_sets sets{};

        const auto userBuffersCount{buffers.size()};

        // We add tags for different index types, because we won't be able to draw these meshes together
        switch (m_meshes.get_index_type({mesh.value}))
        {
        case mesh_index_type::none:
            sets.tags.add(m_indexNoneTag);
            break;
        case mesh_index_type::u16:
            sets.tags.add(m_indexU16Tag);
            break;
        case mesh_index_type::u32:
            sets.tags.add(m_indexU32Tag);
            break;
        }

        sets.components.add(m_meshComponent);

        for (usize i = 0; i < userBuffersCount; ++i)
        {
            const ecs::component_type component{buffers[i].value};
            sets.components.add(component);
        }

        const auto entity = m_instances.create(sets);

        auto* const components = start_lifetime_as_array<ecs::component_type>(buffers.data(), userBuffersCount);

        // Retrieve the user buffers data
        m_instances.get(entity, {components, userBuffersCount}, outData);

        std::byte* meshComponentPtr;
        m_instances.get(entity, {&m_meshComponent, 1}, {&meshComponentPtr, 1});
        new (meshComponentPtr) mesh_handle{mesh.value};

        return h32<draw_instance>{entity.value};
    }

    void draw_registry::get_instance_data(
        h32<draw_instance> instance, std::span<const h32<draw_buffer>> buffers, std::span<std::byte*> outData)
    {
        const ecs::entity e{instance.value};

        auto* const components = start_lifetime_as_array<ecs::component_type>(buffers.data(), buffers.size());
        m_instances.get(e, {components, buffers.size()}, outData);
    }

    void draw_registry::destroy_instance(h32<draw_instance> instance)
    {
        m_instances.destroy(ecs::entity{instance.value});
    }

    h32<draw_buffer> draw_registry::get_or_register(const draw_buffer& buffer)
    {
        if (const auto id = m_typeRegistry.find_component(type_id{buffer.name}))
        {
            return h32<draw_buffer>{id.value};
        }

        // Store the name in the interner, since these names are anyway used in reflection for render passes,
        // and we need to make sure the string stays alive.
        const h32<string> bufferName{m_interner->get_or_add(buffer.name)};

        const auto id = m_typeRegistry.register_component(ecs::component_type_desc{
            .type = type_id{m_interner->str(bufferName)},
            .size = buffer.elementSize,
            .alignment = buffer.elementAlignment,
        });

        const auto bufferId = h32<draw_buffer>{id.value};
        m_meshNames.emplace(bufferId, bufferName);

        return bufferId;
    }

    h32<string> draw_registry::get_name(h32<draw_buffer> drawBuffer) const
    {
        auto* const str = m_meshNames.try_find(drawBuffer);
        return str ? *str : h32<string>{};
    }

    void draw_registry::generate_mesh_database(frame_allocator& allocator, staging_buffer& stagingBuffer)
    {
        const std::span lookup = m_meshes.create_mesh_table_lookup(allocator);

        if (lookup.empty())
        {
            m_meshTablesBuffer = {};
            m_meshTablesBufferOffset = 0;
            m_meshTablesBufferSize = 0;
            return;
        }

        const auto b = m_storageBuffer.allocate(*m_ctx, u32(lookup.size()));

        m_meshTablesBuffer = b.buffer;
        m_meshTablesBufferOffset = b.offset;
        m_meshTablesBufferSize = b.size;

        [[maybe_unused]] const bool success =
            stagingBuffer.upload(lookup, m_meshTablesBuffer, m_meshTablesBufferOffset);

        OBLO_ASSERT(success);
    }

    void draw_registry::generate_draw_calls(frame_allocator& allocator, staging_buffer& stagingBuffer)
    {
        const std::span archetypes = m_instances.get_archetypes();

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
            const u32 numEntities = ecs::get_entities_count(archetype);

            if (numEntities == 0)
            {
                continue;
            }

            ++currentDrawBatch;

            *currentDrawBatch = {};

            std::span<const std::byte> drawCommands;
            buffer drawCommandsBuffer;

            const std::span componentTypes = ecs::get_component_types(archetype);
            const ecs::component_and_tag_sets typeSets = ecs::get_component_and_tag_sets(archetype);
            const bool isIndexed = !typeSets.tags.contains(m_indexNoneTag);

            VkIndexType vkIndexType = VK_INDEX_TYPE_NONE_KHR;
            buffer indexBuffer{};

            if (!isIndexed)
            {
                const auto commands = allocate_n_span<VkDrawIndirectCommand>(allocator, numEntities);

                drawCommands = std::as_bytes(commands);
                drawCommandsBuffer = m_drawCallsBuffer.allocate(*m_ctx, sizeof(VkDrawIndirectCommand) * numEntities);

                auto* nextCommand = commands.data();

                u32 meshHandleOffset;
                std::byte* meshHandleBegin;

                ecs::for_each_chunk(archetype,
                    {&m_meshComponent, 1},
                    {&meshHandleOffset, 1},
                    {&meshHandleBegin, 1},
                    [&](const ecs::entity*, std::span<std::byte*> componentArrays, u32 numEntitiesInChunk)
                    {
                        auto* const meshHandles =
                            start_lifetime_as_array<mesh_handle>(componentArrays[0], numEntitiesInChunk);

                        for (const auto meshId : std::span{meshHandles, numEntitiesInChunk})
                        {
                            const auto range = m_meshes.get_table_range(meshId);

                            *nextCommand = {
                                .vertexCount = range.vertexCount,
                                .instanceCount = 1,
                                .firstVertex = range.vertexOffset,
                                .firstInstance = 0,
                            };

                            ++nextCommand;
                        }
                    });
            }
            else
            {
                const auto indexedCommands = allocate_n_span<VkDrawIndexedIndirectCommand>(allocator, numEntities);

                drawCommands = std::as_bytes(indexedCommands);
                drawCommandsBuffer =
                    m_drawCallsBuffer.allocate(*m_ctx, sizeof(VkDrawIndexedIndirectCommand) * numEntities);

                auto* nextCommand = indexedCommands.data();

                u32 meshHandleOffset;
                std::byte* meshHandleBegin;

                ecs::for_each_chunk(archetype,
                    {&m_meshComponent, 1},
                    {&meshHandleOffset, 1},
                    {&meshHandleBegin, 1},
                    [&](const ecs::entity*, std::span<std::byte*> componentArrays, u32 numEntitiesInChunk)
                    {
                        auto* const meshHandles =
                            start_lifetime_as_array<mesh_handle>(componentArrays[0], numEntitiesInChunk);

                        for (const auto meshId : std::span{meshHandles, numEntitiesInChunk})
                        {
                            const auto range = m_meshes.get_table_range(meshId);

                            *nextCommand = {
                                .indexCount = range.indexCount,
                                .instanceCount = 1,
                                .firstIndex = range.indexOffset,
                                .vertexOffset = i32(range.vertexOffset),
                                .firstInstance = 0,
                            };

                            ++nextCommand;
                        }
                    });

                mesh_index_type indexType{mesh_index_type::none};

                if (typeSets.tags.contains(m_indexU16Tag))
                {
                    vkIndexType = VK_INDEX_TYPE_UINT16;
                    indexType = mesh_index_type::u16;
                }
                else if (typeSets.tags.contains(m_indexU16Tag))
                {
                    vkIndexType = VK_INDEX_TYPE_UINT32;
                    indexType = mesh_index_type::u32;
                }

                indexBuffer = m_meshes.get_index_buffer(indexType);

                OBLO_ASSERT(vkIndexType != VK_INDEX_TYPE_NONE_KHR);
            }

            stagingBuffer.upload(drawCommands, drawCommandsBuffer.buffer, drawCommandsBuffer.offset);
            ++drawBatches;

            currentDrawBatch->drawCommands = {
                .buffer = drawCommandsBuffer.buffer,
                .offset = drawCommandsBuffer.offset,
                .drawCount = numEntities,
                .isIndexed = isIndexed,
                .indexBuffer = indexBuffer.buffer,
                .indexBufferOffset = indexBuffer.offset,
                .indexType = vkIndexType,
            };

            // TODO: Don't blindly update all instance buffers every frame
            // Update instance buffers

            const auto componentsCount = componentTypes.size();

            draw_instance_buffers instanceBuffers{
                .bindings = allocate_n<h32<vk::draw_buffer>>(allocator, componentsCount),
                .buffers = allocate_n<vk::buffer>(allocator, componentsCount),
                .count = u32(componentsCount),
            };

            const auto dataOffsets = allocate_n_span<u32>(allocator, componentsCount);
            const auto componentData = allocate_n_span<std::byte*>(allocator, componentsCount);

            for (usize i = 0; i < componentsCount; ++i)
            {
                const auto& typeDesc = m_typeRegistry.get_component_type_desc(componentTypes[i]);
                const u32 bufferSize = typeDesc.size * numEntities;

                instanceBuffers.buffers[i] = m_storageBuffer.allocate(*m_ctx, bufferSize);
                instanceBuffers.bindings[i] = h32<draw_buffer>{componentTypes[i].value};
            }

            u32 numProcessedEntities{0};

            ecs::for_each_chunk(archetype,
                componentTypes,
                dataOffsets,
                componentData,
                [&](const ecs::entity*, std::span<std::byte*> componentArrays, u32 numEntitiesInChunk)
                {
                    for (usize i = 0; i < componentsCount; ++i)
                    {
                        // TODO: Maybe skip some special components?
                        const auto& typeDesc = m_typeRegistry.get_component_type_desc(componentTypes[i]);

                        const u32 chunkSize = typeDesc.size * numEntitiesInChunk;
                        const u32 dstOffset = typeDesc.size * numProcessedEntities;

                        const auto& instanceBuffer = instanceBuffers.buffers[i];

                        stagingBuffer.upload({componentArrays[i], chunkSize},
                            instanceBuffer.buffer,
                            instanceBuffer.offset + dstOffset);
                    }

                    numProcessedEntities += numEntitiesInChunk;
                });

            currentDrawBatch->instanceBuffers = instanceBuffers;
        }

        m_drawData = frameDrawData;
        m_drawDataCount = drawBatches;
    }

    std::span<const batch_draw_data> draw_registry::get_draw_calls() const
    {
        return {m_drawData, m_drawDataCount};
    }

    buffer draw_registry::get_mesh_database_buffer() const
    {
        return {.buffer = m_meshTablesBuffer, .offset = m_meshTablesBufferOffset, .size = m_meshTablesBufferSize};
    }
}