#include <oblo/renderer/draw/mesh_database.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/buffered_array.hpp>
#include <oblo/core/frame_allocator.hpp>
#include <oblo/core/iterator/reverse_iterator.hpp>
#include <oblo/core/suballocation/buffer_table.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/gpu/gpu_instance.hpp>
#include <oblo/log/log.hpp>
#include <oblo/renderer/draw/mesh_table.hpp>

#include <array>
#include <memory>

namespace oblo
{
    namespace
    {
        constexpr u64 make_table_id(u32 meshAttributesMask, gpu::mesh_index_type indexType)
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

        std::span<const buffer_table_column_description> attributes_to_buffer_columns(u32 meshAttributesMask,
            std::span<const buffer_table_column_description> attributeDescs,
            std::span<buffer_table_column_description> columns)
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

        std::span<const h32<buffer_table_name>> attributes_to_names(std::span<const u32> attributeIds,
            std::span<const buffer_table_column_description> attributeDescs,
            std::span<h32<buffer_table_name>> columns)
        {
            usize n = 0;

            for (u32 attributeId : attributeIds)
            {
                columns[n] = attributeDescs[attributeId].name;
                ++n;
            }

            return columns.subspan(0, n);
        }

        u8 get_index_byte_size(gpu::mesh_index_type indexType)
        {
            using gpu::mesh_index_type;

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

        // TODO: For now we simply allow a set number of tables using the same index buffer
        constexpr u32 MaxTables{4};

        u64 calculate_table_index_buffer_size(gpu::mesh_index_type indexType, u64 tableIndexCount)
        {
            return get_index_byte_size(indexType) * tableIndexCount;
        }
    }

    struct mesh_database::table
    {
        u64 id;
        u32 globalIndexOffset;
        gpu::mesh_index_type indexType;
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

        m_gpu = &initializer.gpu;

        m_vertexBufferUsage = initializer.vertexBufferUsage;
        m_indexBufferUsage = initializer.indexBufferUsage;
        m_meshBufferUsage = initializer.meshBufferUsage;
        m_tableIndexCount = narrow_cast<u32>(initializer.tableIndexCount);
        m_tableVertexCount = narrow_cast<u32>(initializer.tableVertexCount);
        m_tableMeshCount = narrow_cast<u32>(initializer.tableMeshCount);
        m_tableMeshletCount = narrow_cast<u32>(initializer.tableMeshletCount);

        m_attributes = {};

        const std::span meshDataColumns{start_lifetime_as<buffer_table_column_description>(initializer.meshData.data()),
            initializer.meshData.size()};

        const std::span attributesColumns{
            start_lifetime_as<buffer_table_column_description>(initializer.attributes.data()),
            initializer.attributes.size()};

        m_attributes.assign(attributesColumns.begin(), attributesColumns.end());
        m_meshData.assign(meshDataColumns.begin(), meshDataColumns.end());

        return true;
    }

    void mesh_database::shutdown()
    {
        for (auto& table : m_tables)
        {
            table.meshes->shutdown(*m_gpu);
        }

        m_tables.clear();

        for (auto& bufferPools : m_indexBuffers)
        {
            if (bufferPools.handle)
            {
                m_gpu->destroy(bufferPools.handle);
                bufferPools = {};
            }
        }
    }

    mesh_handle mesh_database::create_mesh(
        u32 meshAttributesMask, gpu::mesh_index_type indexType, u32 vertexCount, u32 indexCount, u32 meshletCount)
    {
        if (vertexCount > m_tableVertexCount || indexCount > m_tableIndexCount || meshletCount > m_tableMeshCount)
        {
            return {};
        }

        const auto tableId = make_table_id(meshAttributesMask, indexType);

        auto it = std::find_if(rbegin(m_tables), rend(m_tables), [tableId](const table& t) { return t.id == tableId; });

        if (it == rend(m_tables))
        {
            auto& newTable = m_tables.emplace_back();
            newTable.id = tableId;
            newTable.indexType = indexType;
            newTable.meshes = std::make_unique<mesh_table>();

            std::array<buffer_table_column_description, MaxAttributes> columnsBuffer;
            const std::span columns = attributes_to_buffer_columns(meshAttributesMask, m_attributes, columnsBuffer);

            u32 indexByteSize{};
            gpu::buffer_range indexBuffer{};

            if (indexType != gpu::mesh_index_type::none)
            {
                indexByteSize = get_index_byte_size(indexType);
                indexBuffer = allocate_index_buffer(indexType);

                newTable.globalIndexOffset = narrow_cast<u32>(indexBuffer.offset / indexByteSize);
            }

            const auto success = newTable.meshes->init(*m_gpu,
                columns,
                m_meshData,
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

            it = rbegin(m_tables);
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
            log::debug("Failed to allocate mesh with [vertices: {}, indices: {}, meshlets: {}]",
                vertexCount,
                indexCount,
                meshletCount);
            return {};
        }

        return make_mesh_handle(u32(&*it - m_tables.data()), outHandle[0]);
    }

    bool mesh_database::fetch_buffers(mesh_handle mesh,
        std::span<const u32> vertexAttributes,
        std::span<gpu::buffer_range> vertexBuffers,
        gpu::buffer_range* indexBuffer,
        std::span<const h32<buffer_table_name>> meshBufferNames,
        std::span<gpu::buffer_range> meshBuffers,
        gpu::buffer_range* meshletsBuffer) const
    {
        const auto [tableId, meshId] = parse_mesh_handle(mesh);

        std::array<h32<buffer_table_name>, MaxAttributes> namesBuffer;
        const std::span vertexAttributeNames = attributes_to_names(vertexAttributes, m_attributes, namesBuffer);

        return m_tables[tableId].meshes->fetch_buffers(mesh_table_entry_id{meshId},
            vertexAttributeNames,
            vertexBuffers,
            indexBuffer,
            meshBufferNames,
            meshBuffers,
            meshletsBuffer);
    }

    gpu::mesh_index_type mesh_database::get_index_type(mesh_handle mesh) const
    {
        const auto [tableId, meshId] = parse_mesh_handle(mesh);
        return m_tables[tableId].meshes->get_index_type();
    }

    gpu::buffer_range mesh_database::get_index_buffer(gpu::mesh_index_type meshIndexType) const
    {
        if (meshIndexType == gpu::mesh_index_type::none)
        {
            return {};
        }

        auto& pool = m_indexBuffers[u32(meshIndexType) - 1];

        return gpu::buffer_range{
            .buffer = pool.handle,
            .offset = 0u,
            .size = pool.handle ? calculate_table_index_buffer_size(meshIndexType, m_tableIndexCount) * MaxTables : 0,
        };
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
            h64<gpu::device_address> vertexDataAddress;
            h64<gpu::device_address> indexDataAddress;
            h64<gpu::device_address> meshDataAddress;
            h64<gpu::device_address> meshletDataAddress;
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

            if (t.indexType != gpu::mesh_index_type::none)
            {
                const gpu::buffer_range indexBuffer = t.meshes->index_buffer();
                const h64 bufferAddress = m_gpu->get_device_address(indexBuffer.buffer);
                gpuTable.indexDataAddress = gpu::offset_device_address(bufferAddress, indexBuffer.offset);
            }

            if (const h32 meshlet = t.meshes->meshlet_buffer())
            {
                gpuTable.meshletDataAddress = m_gpu->get_device_address(meshlet);
            }

            if (const auto buffers = t.meshes->vertex_attribute_buffer_subranges(); !buffers.empty())
            {
                gpuTable.vertexDataAddress = m_gpu->get_device_address(t.meshes->vertex_buffer());
                gpuTable.mask = get_mesh_attribute_mask_from_id(t.id);

                for (u32 v = 0; v < buffers.size(); ++v)
                {
                    gpuTable.attributeOffsets[v] = narrow_cast<u32>(buffers[v].begin);
                }
            }

            if (const auto buffers = t.meshes->mesh_data_buffer_subranges(); !buffers.empty())
            {
                gpuTable.meshDataAddress = m_gpu->get_device_address(t.meshes->mesh_data_buffer());

                for (u32 v = 0; v < buffers.size(); ++v)
                {
                    gpuTable.meshDataOffsets[v] = narrow_cast<u32>(buffers[v].begin);
                }
            }
        }

        return std::as_bytes(gpuTables);
    }

    gpu::buffer_range mesh_database::allocate_index_buffer(gpu::mesh_index_type indexType)
    {
        OBLO_ASSERT(indexType != gpu::mesh_index_type::none);
        auto& pool = m_indexBuffers[u32(indexType) - 1];

        if (!pool.handle)
        {
            const u64 tableByteSize = calculate_table_index_buffer_size(indexType, m_tableIndexCount);

            const expected newBuffer = m_gpu->create_buffer({
                .size = tableByteSize * MaxTables,
                .memoryProperties = {gpu::memory_usage::gpu_only},
                .usages = m_indexBufferUsage,
            });

            if (!newBuffer)
            {
                newBuffer.assert_value();
                return {};
            }

            pool.handle = *newBuffer;
            pool.freeList.reserve(MaxTables);

            for (u32 i = 0; i < MaxTables; ++i)
            {
                pool.freeList.emplace_back(i * tableByteSize, tableByteSize);
            }
        }

        OBLO_ASSERT(!pool.freeList.empty(), "Ran out of tables, could bump MaxTables or implement growth");

        const auto range = pool.freeList.back();
        pool.freeList.pop_back();

        return {.buffer = pool.handle, .offset = range.begin, .size = range.size};
    }
}