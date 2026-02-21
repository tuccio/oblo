#include <oblo/renderer/draw/mesh_table.hpp>

#include <oblo/core/finally.hpp>
#include <oblo/gpu/gpu_instance.hpp>
#include <oblo/gpu/gpu_queue_context.hpp>

namespace oblo
{
    namespace
    {
        u64 compute_buffer_size(std::span<const buffer_table_column_description> columns, u32 multiplier, u32 alignment)
        {
            u64 maxBufferSize = narrow_cast<u32>((alignment - 1) * columns.size());

            for (const auto& column : columns)
            {
                maxBufferSize += column.elementSize * multiplier;
            }

            return maxBufferSize;
        }

        constexpr auto MeshletRangeName = h32<buffer_table_name>{1};
    }

    bool mesh_table::init(gpu::gpu_instance& gpu,
        std::span<const buffer_table_column_description> vertexAttributes,
        std::span<const buffer_table_column_description> meshAttributes,
        flags<gpu::buffer_usage> vertexBufferUsage,
        flags<gpu::buffer_usage> meshBufferUsage,
        u32 indexByteSize,
        u32 numVertices,
        u32 numIndices,
        u32 numMeshes,
        u32 numMeshlets,
        const gpu::buffer_range& indexBuffer)
    {
        // TODO: Actually read the correct value for the usage
        constexpr u32 alignment = 16u;

        const u64 vertexBufferSize = compute_buffer_size(vertexAttributes, numVertices, alignment);

        // Cleanup if we early return failure
        auto cleanup = finally_if_not_cancelled([this, &gpu] { shutdown(gpu); });

        if (!m_vertexTable.init(0u, vertexBufferSize, vertexAttributes, numVertices, alignment))
        {
            return false;
        }

        const expected vertexBuffer = gpu.create_buffer({
            .size = vertexBufferSize,
            .memoryFlags = gpu::memory_usage::gpu_only,
            .usages = vertexBufferUsage,
            .debugLabel = "mesh_table_vertex_attributes",
        });

        if (!vertexBuffer)
        {
            return false;
        }

        m_vertexBuffer = *vertexBuffer;

        if (!meshAttributes.empty())
        {
            const u64 meshDataBufferSize = compute_buffer_size(meshAttributes, numMeshes, alignment);

            if (!!m_meshDataTable.init(0u, meshDataBufferSize, meshAttributes, numMeshes, alignment))
            {
                return false;
            }

            const expected meshDataBuffer = gpu.create_buffer({
                .size = meshDataBufferSize,
                .memoryFlags = gpu::memory_usage::gpu_only,
                .usages = meshBufferUsage,
                .debugLabel = "mesh_table_per_mesh_data",
            });

            if (!meshDataBuffer)
            {
                return false;
            }

            m_meshDataBuffer = *meshDataBuffer;
        }

        if (numMeshlets > 0)
        {
            const u32 meshletsBufferSize = sizeof(meshlet_range) * numMeshlets;

            const expected meshletsBuffer = gpu.create_buffer({
                .size = meshletsBufferSize,
                .memoryFlags = gpu::memory_usage::gpu_only,
                .usages = meshBufferUsage,
                .debugLabel = "mesh_table_per_mesh_data",
            });

            if (!meshletsBuffer)
            {
                return false;
            }

            m_meshletsBuffer = *meshletsBuffer;

            const buffer_table_column_description desc[]{
                {
                    .name = MeshletRangeName,
                    .elementSize = sizeof(meshlet_range),
                },
            };

            if (m_meshletsTable.init(0u, meshletsBufferSize, desc, numMeshlets, alignment))
            {
                return false;
            }
        }

        m_indexByteSize = indexByteSize;

        switch (m_indexByteSize)
        {
        case 0:
            m_indexType = gpu::mesh_index_type::none;
            break;

        case 1:
            m_indexType = gpu::mesh_index_type::u8;
            break;

        case 2:
            m_indexType = gpu::mesh_index_type::u16;
            break;

        case 4:
            m_indexType = gpu::mesh_index_type::u32;
            break;

        default:
            OBLO_ASSERT(false, "Unsupported mesh index type");
            return false;
        }

        const auto indexBufferSize = u32(numIndices * m_indexByteSize);

        if (indexBuffer.size < indexBufferSize)
        {
            return false;
        }

        if (indexBufferSize != 0)
        {
            m_indexBuffer = {
                .buffer = indexBuffer.buffer,
                .offset = indexBuffer.offset,
                .size = indexBufferSize,
            };

            m_totalIndices = numIndices;
        }
        else
        {
            m_totalIndices = 0u;
        }

        m_firstFreeVertex = 0u;
        m_firstFreeIndex = 0u;
        m_firstFreeMesh = 0u;
        m_firstFreeMeshlet = 0u;

        // We succeeded, cancel the cleanup
        cleanup.cancel();

        return true;
    }

    void mesh_table::shutdown(gpu::gpu_instance& gpu)
    {
        m_vertexTable.shutdown();
        m_meshDataTable.shutdown();
        m_meshletsTable.shutdown();

        // We don't own the index buffer;
        m_indexBuffer = {};

        if (m_vertexBuffer)
        {
            gpu.destroy_buffer(m_vertexBuffer);
            m_vertexBuffer = {};
        }

        if (m_meshDataBuffer)
        {
            gpu.destroy_buffer(m_meshDataBuffer);
            m_meshDataBuffer = {};
        }

        if (m_meshletsBuffer)
        {
            gpu.destroy_buffer(m_meshletsBuffer);
            m_meshletsBuffer = {};
        }

        m_ranges.clear();
        m_firstFreeIndex = 0u;
        m_firstFreeVertex = 0u;
    }

    bool mesh_table::fetch_buffers(mesh_table_entry_id mesh,
        std::span<const h32<buffer_table_name>> vertexBufferNames,
        std::span<gpu::buffer_range> vertexBuffers,
        gpu::buffer_range* indexBuffer,
        std::span<const h32<buffer_table_name>> meshDataNames,
        std::span<gpu::buffer_range> meshDataBuffers,
        gpu::buffer_range* meshletBuffer) const
    {
        const auto* range = m_ranges.try_find(mesh);

        if (!range)
        {
            return false;
        }

        {
            // Handle vertex buffers
            OBLO_ASSERT(vertexBufferNames.size() == vertexBuffers.size())

            const std::span allBuffers = m_vertexTable.buffer_subranges();
            const std::span elementSizes = m_vertexTable.element_sizes();

            for (usize i = 0; i < vertexBufferNames.size(); ++i)
            {
                const auto columnIndex = m_vertexTable.find(vertexBufferNames[i]);

                gpu::buffer_range result{};

                if (columnIndex >= 0)
                {
                    const auto elementSize = elementSizes[columnIndex];

                    const buffer_table_subrange& subrange = allBuffers[columnIndex];

                    result.buffer = m_vertexBuffer;
                    result.offset = subrange.begin + elementSize * range->vertexOffset;
                    result.size = elementSize * range->vertexCount;
                }

                vertexBuffers[i] = result;
            }
        }

        {
            // Handle mesh data
            OBLO_ASSERT(meshDataNames.size() == meshDataBuffers.size())

            const std::span allBuffers = m_meshDataTable.buffer_subranges();
            const std::span elementSizes = m_meshDataTable.element_sizes();

            for (usize i = 0; i < meshDataNames.size(); ++i)
            {
                const auto columnIndex = m_meshDataTable.find(meshDataNames[i]);

                gpu::buffer_range result{};

                if (columnIndex >= 0)
                {
                    const auto elementSize = elementSizes[columnIndex];

                    const buffer_table_subrange& subrange = allBuffers[columnIndex];

                    result.buffer = m_meshDataBuffer;
                    result.offset = subrange.begin + elementSize * range->vertexOffset;
                    result.size = elementSize * range->vertexCount;
                }

                meshDataBuffers[i] = result;
            }
        }

        if (indexBuffer)
        {
            *indexBuffer = m_indexBuffer;
            indexBuffer->offset += m_indexByteSize * range->indexOffset;
            indexBuffer->size = m_indexByteSize * range->indexCount;
        }

        if (meshletBuffer && m_meshletsBuffer)
        {
            *meshletBuffer = {
                .buffer = m_meshDataBuffer,
            };

            meshletBuffer->offset += sizeof(meshlet_range) * range->meshletOffset;
            meshletBuffer->size = sizeof(meshlet_range) * range->meshletCount;
        }

        return true;
    }

    void mesh_table::fetch_buffers(std::span<const h32<buffer_table_name>> names,
        std::span<gpu::buffer_range> vertexBuffers,
        gpu::buffer_range* indexBuffer) const
    {
        const std::span allBuffers = m_vertexTable.buffer_subranges();

        for (usize i = 0; i < names.size(); ++i)
        {
            const auto columnIndex = m_vertexTable.find(names[i]);

            if (columnIndex >= 0)
            {
                const buffer_table_subrange subrange = allBuffers[columnIndex];

                vertexBuffers[i] = {
                    .buffer = m_vertexBuffer,
                    .offset = subrange.begin,
                    .size = subrange.end - subrange.begin,
                };
            }
        }

        if (indexBuffer)
        {
            *indexBuffer = m_indexBuffer;
        }
    }

    bool mesh_table::allocate_meshes(std::span<const mesh_table_entry> meshes,
        std::span<mesh_table_entry_id> outHandles)
    {
        OBLO_ASSERT(meshes.size() == outHandles.size());

        u32 numVertices{0};
        u32 numIndices{0};
        u32 numMeshlets{0};

        for (const auto& mesh : meshes)
        {
            numVertices += mesh.vertexCount;
            numIndices += mesh.indexCount;
            numMeshlets += mesh.meshletCount;
        }

        const auto newVertexEnd = m_firstFreeVertex + numVertices;
        const auto newIndexEnd = m_firstFreeIndex + numIndices;
        const auto newMeshletEnd = m_firstFreeMeshlet + numMeshlets;

        // If we have no mesh data to attach, we can ignore allocations for m_meshDataTable
        const u32 meshDataRow = m_meshDataTable.rows_count() == 0 ? 0 : 1;
        const auto newMeshesEnd = m_firstFreeMesh + meshDataRow * meshes.size();

        const auto outOfVertexSpace = newVertexEnd > m_vertexTable.rows_count();
        const auto outOfMeshSpace = newMeshesEnd > m_meshDataTable.rows_count();
        const auto outOfIndexSpace = newIndexEnd > m_totalIndices;
        const auto outOfMeshletSpace = newMeshletEnd > m_meshletsTable.rows_count();

        if (outOfVertexSpace || outOfMeshSpace || outOfIndexSpace || outOfMeshletSpace)
        {
            return false;
        }

        bool allSucceeded = true;

        auto outIt = outHandles.begin();

        for (const auto [meshVertices, meshIndices, meshletCount] : meshes)
        {
            const auto [it, key] = m_ranges.emplace(buffer_range{
                .vertexOffset = m_firstFreeVertex,
                .vertexCount = meshVertices,
                .indexOffset = m_firstFreeIndex,
                .indexCount = meshIndices,
                .meshOffset = m_firstFreeMesh,
                .meshletOffset = m_firstFreeMeshlet,
                .meshletCount = meshletCount,
            });

            if (key)
            {
                m_firstFreeVertex += meshVertices;
                m_firstFreeIndex += meshIndices;
                m_firstFreeMesh += meshDataRow;
                m_firstFreeMeshlet += meshletCount;
                *outIt = key;
            }

            ++outIt;

            allSucceeded &= bool{key};
        }

        return allSucceeded;
    }

    gpu::buffer_range mesh_table::index_buffer() const
    {
        return m_indexBuffer;
    }

    h32<gpu::buffer> mesh_table::meshlet_buffer() const
    {
        return m_meshletsBuffer;
    }

    i32 mesh_table::find_vertex_attribute(h32<buffer_table_name> name) const
    {
        return m_vertexTable.find(name);
    }

    u32 mesh_table::vertex_count() const
    {
        return m_firstFreeVertex;
    }

    u32 mesh_table::index_count() const
    {
        return m_firstFreeIndex;
    }

    u32 mesh_table::meshes_count() const
    {
        return u32(m_ranges.size());
    }

    gpu::mesh_index_type mesh_table::get_index_type() const
    {
        return m_indexType;
    }

    mesh_table::buffer_range mesh_table::get_mesh_range(mesh_table_entry_id mesh) const
    {
        auto* const range = m_ranges.try_find(mesh);

        if (!range)
        {
            return {};
        }

        return *range;
    }
}