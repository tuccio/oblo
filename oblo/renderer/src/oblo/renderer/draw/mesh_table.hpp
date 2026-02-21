#pragma once

#include <oblo/core/handle_flat_pool_map.hpp>
#include <oblo/core/suballocation/buffer_table.hpp>
#include <oblo/gpu/forward.hpp>
#include <oblo/gpu/structs.hpp>

namespace oblo
{
    struct mesh_table_entry
    {
        u32 vertexCount;
        u32 indexCount;
        u32 meshletCount;
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
            u32 meshletOffset;
            u32 meshletCount;
        };

        struct meshlet_range
        {
            u32 vertexOffset;
            u32 vertexCount;
            u32 indexOffset;
            u32 indexCount;
        };

    public:
        mesh_table() = default;
        mesh_table(const mesh_table&) = delete;
        mesh_table(mesh_table&&) noexcept = delete;
        mesh_table& operator=(const mesh_table&) = delete;
        mesh_table& operator=(mesh_table&&) noexcept = delete;
        ~mesh_table() = default;

        [[nodiscard]] bool init(gpu::gpu_instance& gpu,
            std::span<const buffer_table_column_description> vertexAttributes,
            std::span<const buffer_table_column_description> meshAttributes,
            flags<gpu::buffer_usage> bufferUsage,
            flags<gpu::buffer_usage> meshBufferUsage,
            u32 indexByteSize,
            u32 numVertices,
            u32 numIndices,
            u32 numMeshes,
            u32 numMeshlets,
            const gpu::buffer_range& indexBuffer);

        void shutdown(gpu::gpu_instance& ctx);

        bool fetch_buffers(mesh_table_entry_id mesh,
            std::span<const h32<buffer_table_name>> vertexBufferNames,
            std::span<gpu::buffer_range> vertexBuffers,
            gpu::buffer_range* indexBuffer,
            std::span<const h32<buffer_table_name>> meshDataNames,
            std::span<gpu::buffer_range> meshDataBuffers,
            gpu::buffer_range* meshletBuffer) const;

        void fetch_buffers(std::span<const h32<buffer_table_name>> names,
            std::span<gpu::buffer_range> vertexBuffers,
            gpu::buffer_range* indexBuffer) const;

        bool allocate_meshes(std::span<const mesh_table_entry> meshes, std::span<mesh_table_entry_id> outHandles);

        gpu::buffer_range index_buffer() const;
        h32<gpu::buffer> meshlet_buffer() const;

        u32 vertex_count() const;
        u32 index_count() const;
        u32 meshes_count() const;

        i32 find_vertex_attribute(h32<buffer_table_name> name) const;

        gpu::mesh_index_type get_index_type() const;

        buffer_range get_mesh_range(mesh_table_entry_id mesh) const;

    private:
        static constexpr u32 GenIdBits = 0;

    private:
        buffer_table m_vertexTable;
        buffer_table m_meshDataTable;
        buffer_table m_meshletsTable;
        h32_flat_pool_dense_map<mesh_table_entry, buffer_range, GenIdBits> m_ranges;
        u32 m_firstFreeVertex{0u};
        u32 m_firstFreeIndex{0u};
        u32 m_firstFreeMesh{0u};
        u32 m_firstFreeMeshlet{0u};
        u32 m_totalIndices{0u};
        u32 m_indexByteSize{0u};
        gpu::buffer_range m_indexBuffer{};
        h32<gpu::buffer> m_vertexBuffer{};
        h32<gpu::buffer> m_meshDataBuffer{};
        h32<gpu::buffer> m_meshletsBuffer{};
        gpu::mesh_index_type m_indexType{gpu::mesh_index_type::none};
    };
}