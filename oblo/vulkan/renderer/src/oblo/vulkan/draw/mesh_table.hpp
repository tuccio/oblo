#pragma once

#include <oblo/core/handle_flat_pool_map.hpp>
#include <oblo/vulkan/buffer_table.hpp>

namespace oblo::vk
{
    struct buffer;

    struct mesh_table_entry
    {
        u32 numVertices;
        u32 numIndices;
    };

    using mesh_table_entry_id = h32<mesh_table_entry>;

    class mesh_table
    {
    public:
        struct buffer_range
        {
            u32 vertexOffset;
            u32 vertexCount;
            u32 indexOffset;
            u32 indexCount;
            u32 meshOffset;
        };

    public:
        mesh_table() = default;
        mesh_table(const mesh_table&) = delete;
        mesh_table(mesh_table&&) noexcept = delete;
        mesh_table& operator=(const mesh_table&) = delete;
        mesh_table& operator=(mesh_table&&) noexcept = delete;
        ~mesh_table() = default;

        [[nodiscard]] bool init(std::span<const buffer_column_description> vertexAttributes,
            std::span<const buffer_column_description> meshAttributes,
            gpu_allocator& allocator,
            resource_manager& resourceManager,
            VkBufferUsageFlags bufferUsage,
            VkBufferUsageFlags meshBufferUsage,
            u32 indexByteSize,
            u32 numVertices,
            u32 numIndices,
            u32 numMeshes,
            const buffer& indexBuffer);

        void shutdown(gpu_allocator& allocator, resource_manager& resourceManager);

        bool fetch_buffers(const resource_manager& resourceManager,
            mesh_table_entry_id mesh,
            std::span<const h32<string>> vertexBufferNames,
            std::span<buffer> vertexBuffers,
            buffer* indexBuffer,
            std::span<const h32<string>> meshDataNames,
            std::span<buffer> meshDataBuffers) const;

        void fetch_buffers(const resource_manager& resourceManager,
            std::span<const h32<string>> names,
            std::span<buffer> vertexBuffers,
            buffer* indexBuffer) const;

        bool allocate_meshes(std::span<const mesh_table_entry> meshes, std::span<mesh_table_entry_id> outHandles);

        std::span<const h32<string>> vertex_attribute_names() const;
        std::span<const h32<buffer>> vertex_attribute_buffers() const;
        std::span<const u32> vertex_attribute_element_sizes() const;

        std::span<const h32<buffer>> mesh_buffers() const;

        h32<buffer> index_buffer() const;

        u32 vertex_count() const;
        u32 index_count() const;
        u32 meshes_count() const;

        i32 find_vertex_attribute(h32<string> name) const;

        VkIndexType get_index_type() const;

        buffer_range get_mesh_range(mesh_table_entry_id mesh) const;

    private:
        static constexpr u32 GenIdBits = 0;

    private:
        buffer_table m_vertexTable;
        buffer_table m_meshDataTable;
        h32_flat_pool_dense_map<mesh_table_entry, buffer_range, GenIdBits> m_ranges;
        u32 m_firstFreeVertex{0u};
        u32 m_firstFreeIndex{0u};
        u32 m_firstFreeMesh{0u};
        u32 m_totalIndices{0u};
        u32 m_indexByteSize{0u};
        h32<buffer> m_indexBuffer{};
        h32<buffer> m_vertexBuffer{};
        h32<buffer> m_meshDataBuffer{};
        VkIndexType m_indexType{VK_INDEX_TYPE_MAX_ENUM};
    };
}