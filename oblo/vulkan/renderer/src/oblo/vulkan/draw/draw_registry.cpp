#include <oblo/vulkan/draw/draw_registry.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/data_format.hpp>
#include <oblo/core/flags.hpp>
#include <oblo/core/stack_allocator.hpp>
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

        constexpr std::string_view MeshBatchPrefix{"$m"};

        constexpr std::string_view get_attribute_name(attribute_kind attribute)
        {
            switch (attribute)
            {
            case attribute_kind::position:
                return "in_Position";
            case attribute_kind::normal:
                return "in_Normal";
            case attribute_kind::uv0:
                return "in_UV0";
            default:
                unreachable();
            }
        }

        using local_mesh_id = h32<string>;

        struct mesh_batch_component
        {
            u32 offset;
        };

        h64<draw_mesh> make_mesh_id(ptrdiff batchOffset, local_mesh_id meshId)
        {
            const u64 batchId{u64(batchOffset) << 32};
            return h64<draw_mesh>{batchId | meshId.value};
        }

        std::pair<ptrdiff, local_mesh_id> parse_mesh_id(h64<draw_mesh> meshId)
        {
            return {ptrdiff(meshId.value >> 32), local_mesh_id{u32(meshId.value)}};
        }
    }

    struct draw_registry::mesh_batch_id
    {
        u8 indicesBytes;
        u8 numAttributes;
        flags<attribute_kind> attributes;

        constexpr auto operator<=>(const mesh_batch_id&) const = default;
    };

    struct draw_registry::mesh_batch
    {
        mesh_batch_id id{};
        std::unique_ptr<mesh_table> table;
        local_mesh_id lastMeshId{};
        ecs::component_type component{};
    };

    draw_registry::draw_registry() = default;

    draw_registry::~draw_registry() = default;

    void draw_registry::init(vulkan_context& ctx, staging_buffer& stagingBuffer, string_interner& interner)
    {
        m_ctx = &ctx;
        m_stagingBuffer = &stagingBuffer;
        m_interner = &interner;

        m_vertexAttributes.resize(MaxAttributesCount);

        for (u32 i = 0; i < MaxAttributesCount; ++i)
        {
            const auto attribute = static_cast<attribute_kind>(i);

            if (attribute == attribute_kind::indices)
            {
                continue;
            }

            data_format format;

            switch (attribute)
            {
            case attribute_kind::position:
            case attribute_kind::normal:
                format = data_format::vec3;
                break;

            case attribute_kind::uv0:
                format = data_format::vec2;
                break;

            default:
                unreachable();
            }

            const auto attributeName = get_attribute_name(attribute);

            const auto [size, alignment] = get_size_and_alignment(format);

            m_vertexAttributes[i] = {
                .name = interner.get_or_add(attributeName),
                .elementSize = u32(size),
            };
        }

        m_instances.init(&m_typeRegistry);

        m_meshBatchComponent = m_typeRegistry.register_component(ecs::make_component_type_desc<mesh_batch_component>());

        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(m_ctx->get_physical_device(), &properties);

        u32 bufferChunkSize{1u << 26};

        m_storageBuffer.init(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            memory_usage::gpu_only,
            narrow_cast<u32>(properties.limits.minStorageBufferOffsetAlignment),
            bufferChunkSize);

        m_drawCallsBuffer.init(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
            memory_usage::gpu_only,
            alignof(VkDrawIndexedIndirectCommand),
            bufferChunkSize);
    }

    void draw_registry::shutdown()
    {
        for (auto& batch : m_meshBatches)
        {
            batch.table->shutdown(m_ctx->get_allocator(), m_ctx->get_resource_manager());
        }
    }

    void draw_registry::end_frame()
    {
        m_drawData = {};
        m_drawDataCount = 0;

        m_storageBuffer.restore_all();
        m_drawCallsBuffer.restore_all();
    }

    h64<draw_mesh> draw_registry::get_or_create_mesh(oblo::resource_registry& resourceRegistry,
        const resource_ref<mesh>& resourceId)
    {
        if (const auto it = m_cachedMeshes.find(resourceId.id); it != m_cachedMeshes.end())
        {
            return it->second;
        }

        const auto anyResource = resourceRegistry.get_resource(resourceId.id);
        const auto meshResource = anyResource.as<mesh>();

        stack_allocator<1024> stackAllocator;

        if (!meshResource)
        {
            return {};
        }

        auto* const meshPtr = meshResource.get();

        const u32 numAttributes = meshPtr->get_attributes_count();

        mesh_batch_id id{};
        buffer_columns columns{};

        auto* const attributeNames = allocate_n<h32<string>>(stackAllocator, numAttributes);
        auto* const attributes = allocate_n<attribute_kind>(stackAllocator, numAttributes);

        u32 vertexAttributesCount{0};

        for (u32 i = 0; i < numAttributes; ++i)
        {
            const auto& meshAttribute = meshPtr->get_attribute_at(i);

            if (const auto kind = meshAttribute.kind; kind != attribute_kind::indices)
            {
                const buffer_column_description& expectedAttribute = m_vertexAttributes[u32(kind)];

                auto& column = columns[vertexAttributesCount];

                OBLO_ASSERT(get_size_and_alignment(meshAttribute.format).first == expectedAttribute.elementSize);
                column = expectedAttribute;

                id.attributes |= kind;

                attributeNames[vertexAttributesCount] = column.name;
                attributes[vertexAttributesCount] = kind;

                ++vertexAttributesCount;
            }
            else
            {
                OBLO_ASSERT(id.indicesBytes == 0, "Two different index buffers?");

                switch (meshAttribute.format)
                {
                case data_format::u8:
                    id.indicesBytes = 1;
                    break;

                case data_format::u16:
                    id.indicesBytes = 2;
                    break;

                case data_format::u32:
                    id.indicesBytes = 4;
                    break;

                default:
                    OBLO_ASSERT(false, "Unhandled index format");
                    break;
                }
            }
        }

        auto* const meshBatch = get_or_create_mesh_batch(id, columns);

        if (!meshBatch)
        {
            return {};
        }

        ++meshBatch->lastMeshId.value;

        const auto newMeshId = meshBatch->lastMeshId;

        const mesh_table_entry meshEntry{
            .id = newMeshId,
            .numVertices = meshPtr->get_vertex_count(),
            .numIndices = meshPtr->get_index_count(),
        };

        if (!meshBatch->table->allocate_meshes({&meshEntry, 1u}))
        {
            return {};
        }

        buffer indexBuffer{};
        auto buffers = allocate_n_span<buffer>(stackAllocator, vertexAttributesCount);

        meshBatch->table->fetch_buffers(m_ctx->get_resource_manager(),
            meshEntry.id,
            {attributeNames, vertexAttributesCount},
            buffers,
            &indexBuffer);

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
            const auto kind = attributes[i];
            const auto data = meshPtr->get_attribute(kind);

            doUpload(data, buffers[i]);
        }

        const auto meshOffset{meshBatch - m_meshBatches.data()};
        const h64<draw_mesh> globalMeshId{make_mesh_id(meshOffset, newMeshId)};
        m_cachedMeshes.emplace(resourceId.id, globalMeshId);

        return globalMeshId;
    }

    const mesh_table* draw_registry::try_get_mesh_table(u32 id) const
    {
        if (id >= m_meshBatches.size())
        {
            return nullptr;
        }

        return m_meshBatches[id].table.get();
    }

    h32<draw_instance> draw_registry::create_instance(
        h64<draw_mesh> mesh, std::span<const h32<draw_buffer>> buffers, std::span<std::byte*> outData)
    {
        OBLO_ASSERT(buffers.size() == outData.size());

        const auto [batchOffset, localMeshid] = parse_mesh_id(mesh);

        ecs::component_and_tags_sets sets{};

        const auto userBuffersCount{buffers.size()};

        const auto localMeshIdComponent = m_meshBatches[batchOffset].component;
        sets.components.add(localMeshIdComponent);
        sets.components.add(m_meshBatchComponent);

        for (usize i = 0; i < userBuffersCount; ++i)
        {
            const ecs::component_type component{buffers[i].value};
            sets.components.add(component);
        }

        const auto entity = m_instances.create(sets);

        auto* const components = start_lifetime_as_array<ecs::component_type>(buffers.data(), userBuffersCount);

        // Retrieve the user buffers data
        m_instances.get(entity, {components, userBuffersCount}, outData);

        std::byte* localMeshIdPtr;
        m_instances.get(entity, {&localMeshIdComponent, 1}, {&localMeshIdPtr, 1});
        new (localMeshIdPtr) local_mesh_id{localMeshid};

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

    draw_registry::mesh_batch* draw_registry::get_or_create_mesh_batch(const mesh_batch_id& batchId,
        std::span<const buffer_column_description> columns)
    {
        for (auto& batch : m_meshBatches)
        {
            if (batch.id == batchId)
            {
                return &batch;
            }
        }

        auto table = std::make_unique<mesh_table>();

        if (!table->init(columns,
                m_ctx->get_allocator(),
                m_ctx->get_resource_manager(),
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                batchId.indicesBytes,
                MaxVerticesPerBatch,
                MaxIndicesPerBatch))
        {
            return nullptr;
        }

        const auto batchOffset = m_meshBatches.size();

        auto& newBatch = m_meshBatches.emplace_back();
        newBatch.id = batchId;
        newBatch.table = std::move(table);

        constexpr u32 BufferSize{128};

        char buffer[BufferSize];
        std::memcpy(buffer, MeshBatchPrefix.data(), MeshBatchPrefix.size());

        const auto [end, ec] = std::to_chars(buffer + MeshBatchPrefix.size(), buffer + BufferSize, batchOffset);

        const h32<string> name{m_interner->get_or_add(std::string_view{buffer, end})};

        auto typeDesc = ecs::make_component_type_desc<local_mesh_id>();
        typeDesc.type = type_id{m_interner->str(name)};

        newBatch.component = m_typeRegistry.register_component(typeDesc);

        return &newBatch;
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

            const std::span componentTypes = ecs::get_component_types(archetype);

            ecs::component_type meshBatchComponent{};

            for (const auto componentType : componentTypes)
            {
                const auto& typeDesc = m_typeRegistry.get_component_type_desc(componentType);

                if (typeDesc.type.name.starts_with(MeshBatchPrefix))
                {
                    meshBatchComponent = componentType;
                    break;
                }
            }

            if (!meshBatchComponent)
            {
                OBLO_ASSERT(false, "A batch without mesh batch component is not expected currently");
                break;
            }

            const auto meshBatchIt = std::find_if(m_meshBatches.begin(),
                m_meshBatches.end(),
                [meshBatchComponent](const mesh_batch& meshBatch)
                { return meshBatch.component == meshBatchComponent; });

            OBLO_ASSERT(meshBatchIt != m_meshBatches.end());

            ++currentDrawBatch;

            *currentDrawBatch = {
                .meshBatch = u32(meshBatchIt - m_meshBatches.begin()),
            };

            std::span<const std::byte> drawCommands;
            buffer drawCommandsBuffer;

            const bool isIndexed = meshBatchIt->table->get_index_type() != VK_INDEX_TYPE_NONE_KHR;

            if (!isIndexed)
            {
                const auto commands = allocate_n_span<VkDrawIndirectCommand>(allocator, numEntities);

                drawCommands = std::as_bytes(commands);
                drawCommandsBuffer = m_drawCallsBuffer.allocate(*m_ctx, sizeof(VkDrawIndirectCommand) * numEntities);

                auto* nextCommand = commands.data();

                u32 meshLocalIdOffset;
                std::byte* meshLocalIdBegin;

                ecs::for_each_chunk(archetype,
                    {&meshBatchComponent, 1},
                    {&meshLocalIdOffset, 1},
                    {&meshLocalIdBegin, 1},
                    [&](const ecs::entity*, std::span<std::byte*> componentArrays, u32 numEntitiesInChunk)
                    {
                        auto* const meshLocalIds =
                            start_lifetime_as_array<h32<string>>(componentArrays[0], numEntitiesInChunk);

                        for (const auto meshId : std::span{meshLocalIds, numEntitiesInChunk})
                        {
                            const auto range = meshBatchIt->table->get_mesh_range(meshId);

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

                u32 meshLocalIdOffset;
                std::byte* meshLocalIdBegin;

                ecs::for_each_chunk(archetype,
                    {&meshBatchComponent, 1},
                    {&meshLocalIdOffset, 1},
                    {&meshLocalIdBegin, 1},
                    [&](const ecs::entity*, std::span<std::byte*> componentArrays, u32 numEntitiesInChunk)
                    {
                        auto* const meshLocalIds =
                            start_lifetime_as_array<h32<string>>(componentArrays[0], numEntitiesInChunk);

                        for (const auto meshId : std::span{meshLocalIds, numEntitiesInChunk})
                        {
                            const auto range = meshBatchIt->table->get_mesh_range(meshId);

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
            }

            stagingBuffer.upload(drawCommands, drawCommandsBuffer.buffer, drawCommandsBuffer.offset);
            ++drawBatches;

            currentDrawBatch->drawCommands = {
                .buffer = drawCommandsBuffer.buffer,
                .offset = drawCommandsBuffer.offset,
                .drawCount = numEntities,
                .isIndexed = isIndexed,
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

                        instanceBuffers.bindings[i] = h32<draw_buffer>{componentTypes[i].value};
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
}