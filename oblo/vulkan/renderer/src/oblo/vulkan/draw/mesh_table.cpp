#include <oblo/vulkan/draw/mesh_table.hpp>

#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/error.hpp>
#include <oblo/vulkan/gpu_allocator.hpp>
#include <oblo/vulkan/resource_manager.hpp>

namespace oblo::vk
{
    namespace
    {
        u32 compute_buffer_size(std::span<const buffer_column_description> columns, u32 multiplier, u32 alignment)
        {
            u32 maxBufferSize = narrow_cast<u32>((alignment - 1) * columns.size());

            for (const auto& column : columns)
            {
                maxBufferSize += column.elementSize * multiplier;
            }

            return maxBufferSize;
        }
    }

    bool mesh_table::init(std::span<const buffer_column_description> vertexAttributes,
        std::span<const buffer_column_description> meshAttributes,
        gpu_allocator& allocator,
        resource_manager& resourceManager,
        VkBufferUsageFlags vertexBufferUsage,
        VkBufferUsageFlags meshBufferUsage,
        u32 indexByteSize,
        u32 numVertices,
        u32 numIndices,
        u32 numMeshes,
        const buffer& indexBuffer)
    {
        // TODO: Actually read the correct value for the usage
        constexpr u32 alignment = 16u;

        m_vertexBuffer = resourceManager.create(allocator,
            {
                .size = compute_buffer_size(vertexAttributes, numVertices, alignment),
                .usage = vertexBufferUsage,
                .memoryUsage = memory_usage::gpu_only,
            });

        if (const auto buffer = resourceManager.get(m_vertexBuffer);
            !m_vertexTable.init(buffer, vertexAttributes, resourceManager, numVertices, alignment))
        {
            shutdown(allocator, resourceManager);
            return false;
        }

        if (!meshAttributes.empty())
        {
            m_meshDataBuffer = resourceManager.create(allocator,
                {
                    .size = compute_buffer_size(meshAttributes, numMeshes, alignment),
                    .usage = meshBufferUsage,
                    .memoryUsage = memory_usage::gpu_only,
                });

            if (const auto buffer = resourceManager.get(m_meshDataBuffer);
                !m_meshDataTable.init(buffer, meshAttributes, resourceManager, numMeshes, alignment))
            {
                shutdown(allocator, resourceManager);
                return false;
            }
        }

        m_indexByteSize = indexByteSize;

        switch (m_indexByteSize)
        {
        case 0:
            m_indexType = VK_INDEX_TYPE_NONE_KHR;
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

        if (indexBuffer.size < indexBufferSize)
        {
            return false;
        }

        if (indexBufferSize != 0)
        {
            m_indexBuffer = resourceManager.register_buffer({
                .buffer = indexBuffer.buffer,
                .offset = indexBuffer.offset,
                .size = indexBufferSize,
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

    void mesh_table::shutdown(gpu_allocator& allocator, resource_manager& resourceManager)
    {
        m_vertexTable.shutdown(resourceManager);
        m_meshDataTable.shutdown(resourceManager);

        if (m_indexBuffer)
        {
            resourceManager.unregister_buffer(m_indexBuffer);
            m_indexBuffer = {};
        }

        if (m_vertexBuffer)
        {
            resourceManager.destroy(allocator, m_vertexBuffer);
            m_vertexBuffer = {};
        }

        if (m_meshDataBuffer)
        {
            resourceManager.destroy(allocator, m_meshDataBuffer);
            m_meshDataBuffer = {};
        }

        m_ranges.clear();
        m_firstFreeIndex = 0u;
        m_firstFreeVertex = 0u;
    }

    bool mesh_table::fetch_buffers(const resource_manager& resourceManager,
        mesh_table_entry_id mesh,
        std::span<const h32<string>> vertexBufferNames,
        std::span<buffer> vertexBuffers,
        buffer* indexBuffer,
        std::span<const h32<string>> meshDataNames,
        std::span<buffer> meshDataBuffers) const
    {
        const auto* range = m_ranges.try_find(mesh);

        if (!range)
        {
            return false;
        }

        {
            // Handle vertex buffers
            OBLO_ASSERT(vertexBufferNames.size() == vertexBuffers.size())

            const std::span allBuffers = m_vertexTable.buffers();
            const std::span elementSizes = m_vertexTable.element_sizes();

            for (usize i = 0; i < vertexBufferNames.size(); ++i)
            {
                const auto columnIndex = m_vertexTable.find(vertexBufferNames[i]);

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
        }

        {
            // Handle mesh data
            OBLO_ASSERT(meshDataNames.size() == meshDataBuffers.size())

            const std::span allBuffers = m_meshDataTable.buffers();
            const std::span elementSizes = m_meshDataTable.element_sizes();

            for (usize i = 0; i < meshDataNames.size(); ++i)
            {
                const auto columnIndex = m_meshDataTable.find(meshDataNames[i]);

                buffer result{};

                if (columnIndex >= 0)
                {
                    const auto elementSize = elementSizes[columnIndex];

                    const auto buffer = allBuffers[columnIndex];
                    result = resourceManager.get(buffer);
                    result.offset += elementSize * range->meshOffset;
                    result.size = elementSize;
                }

                meshDataBuffers[i] = result;
            }
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
        const std::span allBuffers = m_vertexTable.buffers();

        for (usize i = 0; i < names.size(); ++i)
        {
            const auto columnIndex = m_vertexTable.find(names[i]);

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

    bool mesh_table::allocate_meshes(std::span<const mesh_table_entry> meshes,
        std::span<mesh_table_entry_id> outHandles)
    {
        OBLO_ASSERT(meshes.size() == outHandles.size());

        u32 numVertices{0};
        u32 numIndices{0};

        for (const auto& mesh : meshes)
        {
            numVertices += mesh.numVertices;
            numIndices += mesh.numIndices;
        }

        const auto newVertexEnd = m_firstFreeVertex + numVertices;
        const auto newIndexEnd = m_firstFreeIndex + numIndices;
        const auto newMeshesEnd = m_firstFreeMesh + meshes.size();

        if (newVertexEnd > m_vertexTable.rows_count() || newMeshesEnd > m_meshDataTable.rows_count() ||
            newIndexEnd > m_totalIndices)
        {
            return false;
        }

        bool allSucceeded = true;

        auto outIt = outHandles.begin();

        for (const auto [meshVertices, meshIndices] : meshes)
        {
            const auto [it, key] = m_ranges.emplace(buffer_range{
                .vertexOffset = m_firstFreeVertex,
                .vertexCount = meshVertices,
                .indexOffset = m_firstFreeIndex,
                .indexCount = meshIndices,
                .meshOffset = m_firstFreeMesh,
            });

            if (key)
            {
                m_firstFreeVertex += meshVertices;
                m_firstFreeIndex += meshIndices;
                ++m_firstFreeMesh;
                *outIt = key;
            }

            ++outIt;

            allSucceeded &= bool{key};
        }

        return allSucceeded;
    }

    std::span<const h32<string>> mesh_table::vertex_attribute_names() const
    {
        return m_vertexTable.names();
    }

    std::span<const h32<buffer>> mesh_table::vertex_attribute_buffers() const
    {
        return m_vertexTable.buffers();
    }

    std::span<const u32> mesh_table::vertex_attribute_element_sizes() const
    {
        return m_vertexTable.element_sizes();
    }

    i32 mesh_table::find_vertex_attribute(h32<string> name) const
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

    VkIndexType mesh_table::get_index_type() const
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