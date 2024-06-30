#include <oblo/vulkan/draw/mesh_database.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/buffered_array.hpp>
#include <oblo/core/frame_allocator.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/draw/mesh_table.hpp>
#include <oblo/vulkan/gpu_allocator.hpp>
#include <oblo/vulkan/resource_manager.hpp>

#include <array>
#include <memory>

namespace oblo::vk
{
    namespace
    {
        constexpr u64 make_table_id(u32 meshAttributesMask, mesh_index_type indexType)
        {
            return u64{meshAttributesMask} | (u64(indexType) << 32);
        }

        constexpr u32 get_mesh_attribute_mask_from_id(u64 id)
        {
            return u32(id);
        }

        constexpr mesh_handle make_mesh_handle(u32 tableId, mesh_table_entry_id meshId)
        {
            return mesh_handle{(tableId << 24u) | meshId.value};
        }

        constexpr std::pair<u32, mesh_table_entry_id> parse_mesh_handle(const mesh_handle handle)
        {
            const u32 tableId = (handle.value & 0xFF000000) >> 24u;
            const u32 meshId = handle.value & 0x00FFFFFF;
            return {tableId, mesh_table_entry_id{meshId}};
        }

        std::span<const buffer_column_description> attributes_to_buffer_columns(u32 meshAttributesMask,
            std::span<const buffer_column_description> attributeDescs,
            std::span<buffer_column_description> columns)
        {
            u32 n = 0;

            for (u32 i = 0; i < 32; ++i)
            {
                const u32 attributeId = i;
                const u32 attributeMask = 1u << attributeId;
                if (attributeMask & meshAttributesMask)
                {
                    columns[n] = attributeDescs[attributeId];
                    ++n;
                }
            }

            return columns.subspan(0, n);
        }

        std::span<const h32<string>> attributes_to_names(std::span<const u32> attributeIds,
            std::span<const buffer_column_description> attributeDescs,
            std::span<h32<string>> columns)
        {
            usize n = 0;

            for (u32 attributeId : attributeIds)
            {
                columns[n] = attributeDescs[attributeId].name;
                ++n;
            }

            return columns.subspan(0, n);
        }

        u8 get_index_byte_size(mesh_index_type indexType)
        {
            switch (indexType)
            {
            case mesh_index_type::none:
                return 0;
            case mesh_index_type::u8:
                return 1;
            case mesh_index_type::u16:
                return 2;
            case mesh_index_type::u32:
                return 4;
            default:
                unreachable();
            }
        }
    }

    struct mesh_database::table
    {
        u64 id;
        u32 globalIndexOffset;
        mesh_index_type indexType;
        std::unique_ptr<mesh_table> meshes;
    };

    mesh_database::mesh_database() = default;

    mesh_database::~mesh_database() = default;

    bool mesh_database::init(const mesh_database::initializer& initializer)
    {
        if (!m_tables.empty())
        {
            return false;
        }

        if (initializer.attributes.size() > MaxAttributes || initializer.meshData.size() > MaxMeshBuffers)
        {
            return false;
        }

        m_allocator = &initializer.allocator;
        m_resourceManager = &initializer.resourceManager;

        m_vertexBufferUsage = initializer.vertexBufferUsage;
        m_indexBufferUsage = initializer.indexBufferUsage;
        m_meshBufferUsage = initializer.meshBufferUsage;
        m_tableIndexCount = narrow_cast<u32>(initializer.tableIndexCount);
        m_tableVertexCount = narrow_cast<u32>(initializer.tableVertexCount);
        m_tableMeshCount = narrow_cast<u32>(initializer.tableMeshCount);
        m_tableMeshletCount = narrow_cast<u32>(initializer.tableMeshletCount);

        m_attributes = {};

        const std::span meshDataColumns{start_lifetime_as<buffer_column_description>(initializer.meshData.data()),
            initializer.meshData.size()};

        const std::span attributesColumns{start_lifetime_as<buffer_column_description>(initializer.attributes.data()),
            initializer.attributes.size()};

        m_attributes.assign(attributesColumns.begin(), attributesColumns.end());
        m_meshData.assign(meshDataColumns.begin(), meshDataColumns.end());

        return true;
    }

    void mesh_database::shutdown()
    {
        for (auto& table : m_tables)
        {
            table.meshes->shutdown(*m_allocator, *m_resourceManager);
        }

        m_tables.clear();

        for (auto& bufferPools : m_indexBuffers)
        {
            if (bufferPools.handle)
            {
                m_resourceManager->destroy(*m_allocator, bufferPools.handle);
                bufferPools = {};
            }
        }
    }

    mesh_handle mesh_database::create_mesh(
        u32 meshAttributesMask, mesh_index_type indexType, u32 vertexCount, u32 indexCount, u32 meshletCount)
    {
        if (vertexCount > m_tableVertexCount || indexCount > m_tableIndexCount || meshletCount > m_tableMeshCount)
        {
            return {};
        }

        const auto tableId = make_table_id(meshAttributesMask, indexType);

        auto it =
            std::find_if(m_tables.rbegin(), m_tables.rend(), [tableId](const table& t) { return t.id == tableId; });

        if (it == m_tables.rend())
        {
            auto& newTable = m_tables.emplace_back();
            newTable.id = tableId;
            newTable.indexType = indexType;
            newTable.meshes = std::make_unique<mesh_table>();

            std::array<buffer_column_description, MaxAttributes> columnsBuffer;
            const std::span columns = attributes_to_buffer_columns(meshAttributesMask, m_attributes, columnsBuffer);

            u32 indexByteSize{};
            buffer indexBuffer{};

            if (indexType != mesh_index_type::none)
            {
                indexByteSize = get_index_byte_size(indexType);
                indexBuffer = allocate_index_buffer(indexType);

                newTable.globalIndexOffset = indexBuffer.offset / indexByteSize;
            }

            const auto success = newTable.meshes->init(columns,
                m_meshData,
                *m_allocator,
                *m_resourceManager,
                m_vertexBufferUsage,
                m_meshBufferUsage,
                indexByteSize,
                m_tableVertexCount,
                m_tableIndexCount,
                m_tableMeshCount,
                meshletCount > 0 ? m_tableMeshletCount : 0,
                indexBuffer);

            if (!success)
            {
                // TODO: Clean-up allocations (e.g. index buffer)
                OBLO_ASSERT(success, "Allocations might not be cleaned up properly");
                m_tables.pop_back();
                return {};
            }

            it = m_tables.rbegin();
        }

        const mesh_table_entry meshEntry[] = {
            {
                .vertexCount = vertexCount,
                .indexCount = indexCount,
                .meshletCount = meshletCount,
            },
        };

        mesh_table_entry_id outHandle[1];

        if (!it->meshes->allocate_meshes(meshEntry, outHandle))
        {
            OBLO_ASSERT(false, "Failed to allocate mesh");
            return {};
        }

        return make_mesh_handle(u32(&*it - m_tables.data()), outHandle[0]);
    }

    bool mesh_database::fetch_buffers(mesh_handle mesh,
        std::span<const u32> vertexAttributes,
        std::span<buffer> vertexBuffers,
        buffer* indexBuffer,
        std::span<const h32<string>> meshBufferNames,
        std::span<buffer> meshBuffers,
        buffer* meshletsBuffer) const
    {
        const auto [tableId, meshId] = parse_mesh_handle(mesh);

        std::array<h32<string>, MaxAttributes> namesBuffer;
        const std::span vertexAttributeNames = attributes_to_names(vertexAttributes, m_attributes, namesBuffer);

        return m_tables[tableId].meshes->fetch_buffers(*m_resourceManager,
            mesh_table_entry_id{meshId},
            vertexAttributeNames,
            vertexBuffers,
            indexBuffer,
            meshBufferNames,
            meshBuffers,
            meshletsBuffer);
    }

    mesh_index_type mesh_database::get_index_type(mesh_handle mesh) const
    {
        const auto [tableId, meshId] = parse_mesh_handle(mesh);

        const auto indexType = m_tables[tableId].meshes->get_index_type();

        switch (indexType)
        {
        case VK_INDEX_TYPE_NONE_KHR:
            return mesh_index_type::none;

        case VK_INDEX_TYPE_UINT8_EXT:
            return mesh_index_type::u8;

        case VK_INDEX_TYPE_UINT16:
            return mesh_index_type::u16;

        case VK_INDEX_TYPE_UINT32:
            return mesh_index_type::u32;

        default:
            unreachable();
        }
    }

    buffer mesh_database::get_index_buffer(mesh_index_type meshIndexType) const
    {
        if (meshIndexType == mesh_index_type::none)
        {
            return {};
        }

        buffer indexBuffer{};
        auto& pool = m_indexBuffers[u32(meshIndexType) - 1];

        if (pool.handle)
        {
            indexBuffer = m_resourceManager->get(pool.handle);
        }

        return indexBuffer;
    }

    u32 mesh_database::get_table_index(mesh_handle mesh) const
    {
        return parse_mesh_handle(mesh).first;
    }

    mesh_database::table_range mesh_database::get_table_range(mesh_handle mesh) const
    {
        OBLO_ASSERT(mesh);

        const auto [tableId, meshId] = parse_mesh_handle(mesh);
        const auto& table = m_tables[tableId];
        const auto range = table.meshes->get_mesh_range(mesh_table_entry_id{meshId});

        return {
            .vertexOffset = range.vertexOffset,
            .vertexCount = range.vertexCount,
            .indexOffset = range.indexOffset,
            .indexCount = range.indexCount,
            .meshletOffset = range.meshletOffset,
            .meshletCount = range.meshletCount,
        };
    }

    std::span<const std::byte> mesh_database::create_mesh_table_lookup(frame_allocator& allocator) const
    {
        if (m_tables.empty())
        {
            return {};
        }

        struct mesh_table_gpu
        {
            u64 vertexDataAddress;
            u64 indexDataAddress;
            u64 meshDataAddress;
            u64 meshletDataAddress;
            u32 mask;
            u32 indexType;
            u32 attributeOffsets[MaxAttributes];
            u32 meshDataOffsets[MaxMeshBuffers];
        };

        static_assert(sizeof(mesh_table_gpu) % 16 == 0);

        const std::span gpuTables = allocate_n_span<mesh_table_gpu>(allocator, m_tables.size());

        for (usize i = 0; i < m_tables.size(); ++i)
        {
            const auto& t = m_tables[i];

            auto& gpuTable = gpuTables[i];

            gpuTable = {
                .indexType = u32(t.indexType),
            };

            if (t.indexType != mesh_index_type::none)
            {
                const auto indexBufferHandle = t.meshes->index_buffer();
                const auto buffer = m_resourceManager->get(indexBufferHandle);

                const VkBufferDeviceAddressInfo info{
                    .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                    .buffer = buffer.buffer,
                };

                gpuTable.indexDataAddress = vkGetBufferDeviceAddress(m_allocator->get_device(), &info) + buffer.offset;
            }

            if (const auto meshlet = t.meshes->meshlet_buffer())
            {
                const auto buffer = m_resourceManager->get(meshlet);

                const VkBufferDeviceAddressInfo info{
                    .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                    .buffer = buffer.buffer,
                };

                gpuTable.meshletDataAddress =
                    vkGetBufferDeviceAddress(m_allocator->get_device(), &info) + buffer.offset;
            }

            if (const auto buffers = t.meshes->vertex_attribute_buffers(); !buffers.empty())
            {
                const VkBufferDeviceAddressInfo info{
                    .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                    .buffer = m_resourceManager->get(buffers[0]).buffer,
                };

                gpuTable.vertexDataAddress = vkGetBufferDeviceAddress(m_allocator->get_device(), &info);
                gpuTable.mask = get_mesh_attribute_mask_from_id(t.id);

                for (u32 v = 0; v < buffers.size(); ++v)
                {
                    const auto buffer = m_resourceManager->get(buffers[v]);
                    gpuTable.attributeOffsets[v] = buffer.offset;
                    OBLO_ASSERT(buffer.buffer == info.buffer);
                }
            }

            if (const auto buffers = t.meshes->mesh_buffers(); !buffers.empty())
            {
                const VkBufferDeviceAddressInfo info{
                    .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                    .buffer = m_resourceManager->get(buffers[0]).buffer,
                };

                gpuTable.meshDataAddress = vkGetBufferDeviceAddress(m_allocator->get_device(), &info);

                for (u32 v = 0; v < buffers.size(); ++v)
                {
                    const auto buffer = m_resourceManager->get(buffers[v]);
                    gpuTable.meshDataOffsets[v] = buffer.offset;
                    OBLO_ASSERT(buffer.buffer == info.buffer);
                }
            }
        }

        return std::as_bytes(gpuTables);
    }

    buffer mesh_database::allocate_index_buffer(mesh_index_type indexType)
    {
        OBLO_ASSERT(indexType != mesh_index_type::none);
        auto& pool = m_indexBuffers[u32(indexType) - 1];

        if (!pool.handle)
        {
            const u32 indexByteSize = get_index_byte_size(indexType);

            // TODO: For now we simply allow a set number of tables using the same index buffer
            constexpr u32 MaxTables{4};

            const u32 tableByteSize = indexByteSize * m_tableIndexCount;

            pool.handle = m_resourceManager->create(*m_allocator,
                buffer_initializer{
                    .size = tableByteSize * MaxTables,
                    .usage = m_indexBufferUsage,
                    .memoryUsage = memory_usage::gpu_only,
                });

            pool.freeList.reserve(MaxTables);

            for (u32 i = 0; i < MaxTables; ++i)
            {
                pool.freeList.emplace_back(i * tableByteSize, tableByteSize);
            }
        }

        OBLO_ASSERT(!pool.freeList.empty(), "Ran out of tables, could bump MaxTables or implement growth");

        const auto range = pool.freeList.back();
        pool.freeList.pop_back();

        const auto b = m_resourceManager->get(pool.handle);

        return {.buffer = b.buffer, .offset = b.offset + range.begin, .size = range.size};
    }
}