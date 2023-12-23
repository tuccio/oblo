#include <oblo/vulkan/draw/mesh_database.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/frame_allocator.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/vulkan/allocator.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/draw/mesh_table.hpp>
#include <oblo/vulkan/resource_manager.hpp>

#include <bit>
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
            std::span<const mesh_attribute_description> attributeDescs,
            std::span<buffer_column_description> columns)
        {
            u32 n = 0;

            for (u32 i = 0; i < 32; ++i)
            {
                const u32 attributeId = i;
                const u32 attributeMask = 1u << attributeId;
                if (attributeMask & meshAttributesMask)
                {
                    columns[n] = std::bit_cast<buffer_column_description>(attributeDescs[attributeId]);
                    ++n;
                }
            }

            return columns.subspan(0, n);
        }

        std::span<const h32<string>> attributes_to_names(std::span<const u32> attributeIds,
            std::span<const mesh_attribute_description> attributeDescs,
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

        if (initializer.attributes.size() > MaxAttributes)
        {
            return false;
        }

        m_allocator = &initializer.allocator;
        m_resourceManager = &initializer.resourceManager;

        m_bufferUsage = initializer.bufferUsage;
        m_tableIndexCount = initializer.tableIndexCount;
        m_tableVertexCount = initializer.tableVertexCount;

        m_attributes = {};

        std::copy(initializer.attributes.begin(), initializer.attributes.end(), m_attributes.begin());

        return true;
    }

    void mesh_database::shutdown()
    {
        for (auto& table : m_tables)
        {
            table.meshes->shutdown(*m_allocator, *m_resourceManager);
        }

        m_tables.clear();
    }

    mesh_handle mesh_database::create_mesh(
        u32 meshAttributesMask, mesh_index_type indexType, u32 vertexCount, u32 indexCount)
    {
        if (vertexCount > m_tableVertexCount || indexCount > m_tableIndexCount)
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
            newTable.meshes = std::make_unique<mesh_table>();

            std::array<buffer_column_description, MaxAttributes> columnsBuffer;
            const std::span columns = attributes_to_buffer_columns(meshAttributesMask, m_attributes, columnsBuffer);

            const u32 indexByteSize = get_index_byte_size(indexType);
            const buffer indexBuffer = allocate_index_buffer(indexType);

            newTable.globalIndexOffset = indexBuffer.offset / indexByteSize;

            const auto success = newTable.meshes->init(columns,
                *m_allocator,
                *m_resourceManager,
                m_bufferUsage,
                indexByteSize,
                m_tableVertexCount,
                m_tableIndexCount,
                indexBuffer);

            if (!success)
            {
                // TODO: Clean-up allocations (e.g. index buffer)
                m_tables.pop_back();
                return {};
            }

            it = m_tables.rbegin();
        }

        const mesh_table_entry meshEntry[] = {{.numVertices = vertexCount, .numIndices = indexCount}};
        mesh_table_entry_id outHandle[1];

        if (!it->meshes->allocate_meshes(meshEntry, outHandle))
        {
            return {};
        }

        return make_mesh_handle(u32(&*it - m_tables.data()), outHandle[0]);
    }

    bool mesh_database::fetch_buffers(
        mesh_handle mesh, std::span<const u32> attributes, std::span<buffer> vertexBuffers, buffer* indexBuffer) const
    {
        const auto [tableId, meshId] = parse_mesh_handle(mesh);

        std::array<h32<string>, MaxAttributes> namesBuffer;
        const std::span names = attributes_to_names(attributes, m_attributes, namesBuffer);

        return m_tables[tableId].meshes->fetch_buffers(*m_resourceManager,
            mesh_table_entry_id{meshId},
            names,
            vertexBuffers,
            indexBuffer);
    }

    mesh_index_type mesh_database::get_index_type(mesh_handle mesh) const
    {
        const auto [tableId, meshId] = parse_mesh_handle(mesh);

        switch (m_tables[tableId].meshes->get_index_type())
        {
        case VK_INDEX_TYPE_NONE_KHR:
            return mesh_index_type::none;

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
        const auto [tableId, meshId] = parse_mesh_handle(mesh);
        const auto& table = m_tables[tableId];
        const auto range = table.meshes->get_mesh_range(mesh_table_entry_id{meshId});

        return {
            .vertexOffset = range.vertexOffset,
            .vertexCount = range.vertexCount,
            .indexOffset = table.globalIndexOffset + range.indexOffset,
            .indexCount = range.indexCount,
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
            u64 deviceAddress;
            u32 mask;
            u32 padding;
            u32 offsets[MaxAttributes];
        };

        const std::span gpuTables = allocate_n_span<mesh_table_gpu>(allocator, m_tables.size());

        for (usize i = 0; i < m_tables.size(); ++i)
        {
            const auto& t = m_tables[i];

            const auto buffers = t.meshes->vertex_attribute_buffers();

            if (buffers.empty())
            {
                continue;
            }

            const VkBufferDeviceAddressInfo info{
                .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                .buffer = m_resourceManager->get(buffers[0]).buffer,
            };

            auto& gpuTable = gpuTables[i];

            gpuTable = {
                .deviceAddress = vkGetBufferDeviceAddress(m_allocator->get_device(), &info),
                .mask = get_mesh_attribute_mask_from_id(t.id),
            };

            for (u32 v = 0; v < buffers.size(); ++v)
            {
                const auto buffer = m_resourceManager->get(buffers[v]);
                gpuTable.offsets[v] = buffer.offset;
                OBLO_ASSERT(buffer.buffer == info.buffer);
            }
        }

        return std::as_bytes(gpuTables);
    }

    buffer mesh_database::allocate_index_buffer(mesh_index_type indexType)
    {
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
                    .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | m_bufferUsage,
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