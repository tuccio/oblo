#pragma once

#include <oblo/core/handle.hpp>

#include <array>
#include <span>
#include <vector>

#include <vulkan/vulkan_core.h>

namespace oblo
{
    class frame_allocator;
    struct string;
}

namespace oblo::vk
{
    struct buffer;
    struct gpu_mesh;

    class gpu_allocator;
    class mesh_table;
    class resource_manager;

    // We use 24 bits for the mesh index, 8 bits for mesh tables
    using mesh_handle = h32<gpu_mesh>;

    using mesh_table_handle = h8<mesh_table>;

    struct mesh_attribute_description
    {
        h32<string> name;
        u32 elementSize;
    };

    enum class mesh_index_type : u8
    {
        none,
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

        static constexpr u32 MaxAttributes{32};

    public:
        mesh_database();
        mesh_database(const mesh_database&) = delete;
        mesh_database(mesh_database&&) noexcept = delete;

        ~mesh_database();

        mesh_database& operator=(const mesh_database&) = delete;
        mesh_database& operator=(mesh_database&&) noexcept = delete;

        [[nodiscard]] bool init(const initializer& initializer);

        void shutdown();

        mesh_handle create_mesh(u32 meshAttributesMask, mesh_index_type indexType, u32 vertexCount, u32 indexCount);

        bool fetch_buffers(mesh_handle mesh,
            std::span<const u32> attributes,
            std::span<buffer> vertexBuffers,
            buffer* indexBuffer) const;

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
            std::vector<range> freeList;
        };

    private:
        buffer allocate_index_buffer(mesh_index_type indexType);

    private:
        gpu_allocator* m_allocator{};
        resource_manager* m_resourceManager{};
        u32 m_tableVertexCount{};
        u32 m_tableIndexCount{};
        VkBufferUsageFlags m_bufferUsage{};
        std::vector<table> m_tables;
        buffer_pool m_indexBuffers[2];
        std::array<mesh_attribute_description, MaxAttributes> m_attributes{};
    };

    struct mesh_database::initializer
    {
        gpu_allocator& allocator;
        resource_manager& resourceManager;
        std::span<const mesh_attribute_description> attributes;
        VkBufferUsageFlags bufferUsage;
        VkDeviceSize tableVertexCount;
        VkDeviceSize tableIndexCount;
    };

    struct mesh_database::table_range
    {
        u32 vertexOffset;
        u32 vertexCount;
        u32 indexOffset;
        u32 indexCount;
    };
}