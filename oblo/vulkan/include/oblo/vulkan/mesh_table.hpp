#pragma once

#include <oblo/core/flat_dense_map.hpp>
#include <oblo/vulkan/buffer_table.hpp>

namespace oblo::vk
{
    struct buffer;

    struct mesh_table_entry
    {
        h32<string> id;
        u32 numVertices;
        u32 numIndices;
    };

    class mesh_table
    {
    public:
        mesh_table() = default;
        mesh_table(const mesh_table&) = delete;
        mesh_table(mesh_table&&) noexcept = delete;
        mesh_table& operator=(const mesh_table&) = delete;
        mesh_table& operator=(mesh_table&&) noexcept = delete;
        ~mesh_table() = default;

        void init(std::span<const buffer_column_description> columns,
                  allocator& allocator,
                  resource_manager& resourceManager,
                  VkBufferUsageFlags bufferUsage,
                  u32 numVertices,
                  u32 numIndices);

        void shutdown(allocator& allocator, resource_manager& resourceManager);

        bool fetch_buffers(const resource_manager& resourceManager,
                           h32<string> mesh,
                           std::span<const h32<string>> names,
                           std::span<buffer> vertexBuffers,
                           buffer* indexBuffer) const;

        void fetch_buffers(const resource_manager& resourceManager,
                           std::span<const h32<string>> names,
                           std::span<buffer> vertexBuffers,
                           buffer* indexBuffer) const;

        bool allocate_meshes(std::span<const mesh_table_entry> meshes);

        std::span<const h32<string>> vertex_attribute_names() const;
        std::span<const h32<buffer>> vertex_attribute_buffers() const;
        std::span<const u32> vertex_attribute_element_sizes() const;

        u32 vertex_count() const;
        u32 meshes_count() const;

        i32 find_vertex_attribute(h32<string> name) const;

    private:
        struct buffer_range
        {
            u32 vertexOffset;
            u32 vertexCount;
            u32 indexOffset;
            u32 indexCount;
        };

    private:
        buffer_table m_buffers;
        flat_dense_map<h32<string>, buffer_range> m_ranges;
        u32 m_firstFreeVertex{0u};
        u32 m_firstFreeIndex{0u};
        u32 m_totalIndices{0u};
        h32<buffer> m_indexBuffer{};
    };
}