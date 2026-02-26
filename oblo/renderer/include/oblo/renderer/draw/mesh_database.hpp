#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/flags.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/gpu/forward.hpp>
#include <oblo/gpu/enums.hpp>
#include <oblo/renderer/data/handles.hpp>

#include <span>

namespace oblo
{
    namespace gpu::vk
    {
        class gpu_allocator;
    }

    class frame_allocator;

    struct buffer_table_column_description;
    struct buffer_table_name;

    struct mesh_attribute_description
    {
        h32<buffer_table_name> name;
        u64 elementSize;
    };

    // We might want to search all mesh tables that have the same attributes?
    // - Instances could be grouped by mesh table instead
    class mesh_database
    {
    public:
        struct initializer;
        struct table_range;

        static constexpr u32 MaxAttributes{7};
        static constexpr u32 MaxMeshBuffers{3};
        static constexpr u32 MeshIndexTypeCount{u32(gpu::mesh_index_type::u32)};

    public:
        mesh_database();
        mesh_database(const mesh_database&) = delete;
        mesh_database(mesh_database&&) noexcept = delete;

        ~mesh_database();

        mesh_database& operator=(const mesh_database&) = delete;
        mesh_database& operator=(mesh_database&&) noexcept = delete;

        [[nodiscard]] bool init(const initializer& initializer);

        void shutdown();

        mesh_handle create_mesh(
            u32 meshAttributesMask, gpu::mesh_index_type indexType, u32 vertexCount, u32 indexCount, u32 meshletsCount);

        bool fetch_buffers(mesh_handle mesh,
            std::span<const u32> vertexAttributes,
            std::span<gpu::buffer_range> vertexBuffers,
            gpu::buffer_range* indexBuffer,
            std::span<const h32<buffer_table_name>> meshBufferNames,
            std::span<gpu::buffer_range> meshBuffers,
            gpu::buffer_range* meshletsBuffer) const;

        gpu::mesh_index_type get_index_type(mesh_handle mesh) const;

        gpu::buffer_range get_index_buffer(gpu::mesh_index_type indexType) const;

        u32 get_table_index(mesh_handle mesh) const;

        table_range get_table_range(mesh_handle mesh) const;

        std::span<const std::byte> create_mesh_table_lookup(frame_allocator& allocator) const;

    private:
        struct table;

        struct buffer_pool
        {
            struct range
            {
                u64 begin;
                u64 size;
            };

            h32<gpu::buffer> handle{};
            dynamic_array<range> freeList;
        };

    private:
        gpu::buffer_range allocate_index_buffer(gpu::mesh_index_type indexType);

    private:
        gpu::gpu_instance* m_gpu{};
        u32 m_tableVertexCount{};
        u32 m_tableIndexCount{};
        u32 m_tableMeshCount{};
        u32 m_tableMeshletCount{};
        flags<gpu::buffer_usage> m_indexBufferUsage{};
        flags<gpu::buffer_usage> m_vertexBufferUsage{};
        flags<gpu::buffer_usage> m_meshBufferUsage{};
        dynamic_array<table> m_tables;
        buffer_pool m_indexBuffers[MeshIndexTypeCount];
        dynamic_array<buffer_table_column_description> m_attributes{};
        dynamic_array<buffer_table_column_description> m_meshData{};
    };

    struct mesh_database::initializer
    {
        gpu::gpu_instance& gpu;
        std::span<const mesh_attribute_description> attributes;
        std::span<const mesh_attribute_description> meshData;
        flags<gpu::buffer_usage> vertexBufferUsage;
        flags<gpu::buffer_usage> indexBufferUsage;
        flags<gpu::buffer_usage> meshBufferUsage;
        u64 tableVertexCount;
        u64 tableIndexCount;
        u64 tableMeshCount;
        u64 tableMeshletCount;
    };

    struct mesh_database::table_range
    {
        u32 vertexOffset;
        u32 vertexCount;
        u32 indexOffset;
        u32 indexCount;
        u32 meshletOffset;
        u32 meshletCount;
    };
}