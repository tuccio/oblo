#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/vulkan/data/handles.hpp>

#include <span>

#include <vulkan/vulkan_core.h>

namespace oblo
{
    class frame_allocator;
    class string;
}

namespace oblo::vk
{
    struct buffer;
    struct buffer_column_description;

    class gpu_allocator;
    class resource_manager;

    struct mesh_attribute_description
    {
        h32<string> name;
        u32 elementSize;
    };

    enum class mesh_index_type : u8
    {
        none,
        u8,
        u16,
        u32,
    };

    // We might want to search all mesh tables that have the same attributes?
    // - Instances could be grouped by mesh table instead
    class mesh_database
    {
    public:
        struct initializer;
        struct table_range;

        static constexpr u32 MaxAttributes{8};
        static constexpr u32 MaxMeshBuffers{2};
        static constexpr u32 MeshIndexTypeCount{u32(mesh_index_type::u32)};

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
            u32 meshAttributesMask, mesh_index_type indexType, u32 vertexCount, u32 indexCount, u32 meshletsCount);

        bool fetch_buffers(mesh_handle mesh,
            std::span<const u32> vertexAttributes,
            std::span<buffer> vertexBuffers,
            buffer* indexBuffer,
            std::span<const h32<string>> meshBufferNames,
            std::span<buffer> meshBuffers,
            buffer* meshletsBuffer) const;

        mesh_index_type get_index_type(mesh_handle mesh) const;

        buffer get_index_buffer(mesh_index_type indexType) const;

        u32 get_table_index(mesh_handle mesh) const;

        table_range get_table_range(mesh_handle mesh) const;

        std::span<const std::byte> create_mesh_table_lookup(frame_allocator& allocator) const;

    private:
        struct table;

        struct buffer_pool
        {
            struct range
            {
                u32 begin;
                u32 size;
            };

            h32<buffer> handle{};
            dynamic_array<range> freeList;
        };

    private:
        buffer allocate_index_buffer(mesh_index_type indexType);

    private:
        gpu_allocator* m_allocator{};
        resource_manager* m_resourceManager{};
        u32 m_tableVertexCount{};
        u32 m_tableIndexCount{};
        u32 m_tableMeshCount{};
        u32 m_tableMeshletCount{};
        VkBufferUsageFlags m_indexBufferUsage{};
        VkBufferUsageFlags m_vertexBufferUsage{};
        VkBufferUsageFlags m_meshBufferUsage{};
        dynamic_array<table> m_tables;
        buffer_pool m_indexBuffers[MeshIndexTypeCount];
        dynamic_array<buffer_column_description> m_attributes{};
        dynamic_array<buffer_column_description> m_meshData{};
    };

    struct mesh_database::initializer
    {
        gpu_allocator& allocator;
        resource_manager& resourceManager;
        std::span<const mesh_attribute_description> attributes;
        std::span<const mesh_attribute_description> meshData;
        VkBufferUsageFlags vertexBufferUsage;
        VkBufferUsageFlags indexBufferUsage;
        VkBufferUsageFlags meshBufferUsage;
        VkDeviceSize tableVertexCount;
        VkDeviceSize tableIndexCount;
        VkDeviceSize tableMeshCount;
        VkDeviceSize tableMeshletCount;
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