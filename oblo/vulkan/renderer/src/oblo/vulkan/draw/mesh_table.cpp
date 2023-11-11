#include <oblo/vulkan/draw/mesh_table.hpp>

#include <oblo/vulkan/allocator.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/error.hpp>
#include <oblo/vulkan/resource_manager.hpp>

namespace oblo::vk
{
    bool mesh_table::init(std::span<const buffer_column_description> columns,
        allocator& allocator,
        resource_manager& resourceManager,
        VkBufferUsageFlags bufferUsage,
        u32 indexByteSize,
        u32 numVertices,
        u32 numIndices)
    {
        // TODO: Actually read it
        constexpr u32 alignment = 16u;
        u32 maxBufferSize = (alignment - 1) * columns.size();

        for (const auto& column : columns)
        {
            maxBufferSize += column.elementSize * numVertices;
        }

        m_buffer = resourceManager.create(allocator,
            {
                .size = maxBufferSize,
                .usage = bufferUsage,
                .memoryUsage = memory_usage::gpu_only,
            });

        if (!m_buffers.init(resourceManager.get(m_buffer), columns, resourceManager, numVertices, alignment))
        {
            resourceManager.destroy(allocator, m_buffer);
            m_buffer = {};
            return false;
        }

        m_indexByteSize = indexByteSize;

        switch (m_indexByteSize)
        {
        case 0:
            m_indexType = VK_INDEX_TYPE_MAX_ENUM;
            break;

        case 2:
            m_indexType = VK_INDEX_TYPE_UINT16;
            break;

        case 4:
            m_indexType = VK_INDEX_TYPE_UINT32;
            break;

        default:
            OBLO_ASSERT(false, "Unsupported mesh index type");
            return false;
        }

        const auto indexBufferSize = u32(numIndices * m_indexByteSize);

        if (indexBufferSize != 0)
        {
            m_indexBuffer = resourceManager.create(allocator,
                {
                    .size = indexBufferSize,
                    .usage = bufferUsage | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                    .memoryUsage = memory_usage::gpu_only,
                });

            m_totalIndices = numIndices;
        }
        else
        {
            m_totalIndices = 0u;
        }

        m_firstFreeVertex = 0u;
        m_firstFreeIndex = 0u;

        return true;
    }

    void mesh_table::shutdown(allocator& allocator, resource_manager& resourceManager)
    {
        m_buffers.shutdown(resourceManager);

        if (m_indexBuffer)
        {
            resourceManager.destroy(allocator, m_indexBuffer);
            m_indexBuffer = {};
        }

        if (m_buffer)
        {
            resourceManager.destroy(allocator, m_buffer);
            m_buffer = {};
        }

        m_ranges.clear();
        m_firstFreeIndex = 0u;
        m_firstFreeVertex = 0u;
    }

    bool mesh_table::fetch_buffers(const resource_manager& resourceManager,
        h32<string> mesh,
        std::span<const h32<string>> names,
        std::span<buffer> vertexBuffers,
        buffer* indexBuffer) const
    {
        const auto* range = m_ranges.try_find(mesh);

        if (!range)
        {
            return false;
        }

        OBLO_ASSERT(names.size() == vertexBuffers.size())

        const std::span allBuffers = m_buffers.buffers();
        const std::span elementSizes = m_buffers.element_sizes();

        for (usize i = 0; i < names.size(); ++i)
        {
            const auto columnIndex = m_buffers.find(names[i]);

            buffer result{};

            if (columnIndex >= 0)
            {
                const auto elementSize = elementSizes[columnIndex];

                const auto buffer = allBuffers[columnIndex];
                result = resourceManager.get(buffer);
                result.offset += elementSize * range->vertexOffset;
                result.size = elementSize * range->vertexCount;
            }

            vertexBuffers[i] = result;
        }

        if (indexBuffer)
        {
            *indexBuffer = resourceManager.get(m_indexBuffer);
            indexBuffer->offset += m_indexByteSize * range->indexOffset;
            indexBuffer->size = m_indexByteSize * range->indexCount;
        }

        return true;
    }

    void mesh_table::fetch_buffers(const resource_manager& resourceManager,
        std::span<const h32<string>> names,
        std::span<buffer> vertexBuffers,
        buffer* indexBuffer) const
    {
        const std::span allBuffers = m_buffers.buffers();

        for (usize i = 0; i < names.size(); ++i)
        {
            const auto columnIndex = m_buffers.find(names[i]);

            if (columnIndex >= 0)
            {
                const auto buffer = allBuffers[columnIndex];
                vertexBuffers[i] = resourceManager.get(buffer);
            }
        }

        if (indexBuffer)
        {
            *indexBuffer = resourceManager.get(m_indexBuffer);
        }
    }

    bool mesh_table::allocate_meshes(std::span<const mesh_table_entry> meshes)
    {
        u32 numVertices{0};
        u32 numIndices{0};

        for (const auto& mesh : meshes)
        {
            OBLO_ASSERT(mesh.numIndices > 0 && mesh.numVertices > 0);
            numVertices += mesh.numVertices;
            numIndices += mesh.numIndices;
        }

        const auto newVertexEnd = m_firstFreeVertex + numVertices;
        const auto newIndexEnd = m_firstFreeIndex + numIndices;

        if (newVertexEnd > m_buffers.rows_count() || newIndexEnd > m_totalIndices)
        {
            return false;
        }

        bool allSucceeded = true;

        for (const auto [meshId, meshVertices, meshIndices] : meshes)
        {
            const auto [it, ok] = m_ranges.emplace(meshId,
                buffer_range{
                    .vertexOffset = m_firstFreeVertex,
                    .vertexCount = meshVertices,
                    .indexOffset = m_firstFreeIndex,
                    .indexCount = meshIndices,
                });

            if (ok)
            {
                m_firstFreeVertex += meshVertices;
                m_firstFreeIndex += meshIndices;
            }

            allSucceeded &= ok;
        }

        return allSucceeded;
    }

    std::span<const h32<string>> mesh_table::vertex_attribute_names() const
    {
        return m_buffers.names();
    }

    std::span<const h32<buffer>> mesh_table::vertex_attribute_buffers() const
    {
        return m_buffers.buffers();
    }

    std::span<const u32> mesh_table::vertex_attribute_element_sizes() const
    {
        return m_buffers.element_sizes();
    }

    i32 mesh_table::find_vertex_attribute(h32<string> name) const
    {
        return m_buffers.find(name);
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

    VkIndexType mesh_table::get_index_type() const
    {
        return m_indexType;
    }

    mesh_table::buffer_range mesh_table::get_mesh_range(h32<string> mesh) const
    {
        auto* const range = m_ranges.try_find(mesh);

        if (!range)
        {
            return {};
        }

        return *range;
    }
}