#include <oblo/vulkan/draw/mesh_database.hpp>

#include <oblo/core/unreachable.hpp>
#include <oblo/vulkan/draw/mesh_table.hpp>

#include <bit>
#include <memory>

namespace oblo::vk
{
    namespace
    {
        constexpr u64 make_table_id(u32 meshAttributesMask, mesh_index_type indexType)
        {
            return u64{meshAttributesMask} | (u64(indexType) << 32);
        }

        constexpr mesh_handle make_mesh_handle(u32 tableId, u32 meshId)
        {
            return mesh_handle{(tableId << 24u) | meshId};
        }

        constexpr std::pair<u32, u32> parse_mesh_handle(const mesh_handle handle)
        {
            const u32 tableId = (handle.value & 0xFF000000) >> 24u;
            const u32 meshId = handle.value & 0x00FFFFFF;
            return {tableId, meshId};
        }

        std::span<const buffer_column_description> attributes_to_buffer_columns(u32 meshAttributesMask,
            std::span<const mesh_attribute_description> attributeDescs,
            std::span<buffer_column_description> columns)
        {
            u32 n = 0;
            u32 attributeId = 0;

            for (u32 i = 0; i < 32; ++i)
            {
                const u32 attributeMask = 1u << attributeId;
                if (attributeMask & meshAttributesMask)
                {
                    columns[n] = std::bit_cast<buffer_column_description>(attributeDescs[attributeId]);
                    ++n;
                }
            }

            return columns.subspan(0, n);
        }

        std::span<const h32<string>> attributes_to_names(std::span<const u32> attributeIds,
            std::span<const mesh_attribute_description> attributeDescs,
            std::span<h32<string>> columns)
        {
            usize n = 0;

            for (u32 attributeId : attributeIds)
            {
                columns[n] = attributeDescs[attributeId].name;
                ++n;
            }

            return columns.subspan(0, n);
        }

        u8 get_index_byte_size(mesh_index_type indexType)
        {
            switch (indexType)
            {
            case mesh_index_type::none:
                return 0;
            case mesh_index_type::u16:
                return 2;
            case mesh_index_type::u32:
                return 4;
            default:
                unreachable();
            }
        }
    }

    struct mesh_database::table
    {
        u64 id;
        u32 nextMeshId;
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

        if (initializer.attributes.size() > MaxAttributes)
        {
            return false;
        }

        m_allocator = &initializer.allocator;
        m_resourceManager = &initializer.resourceManager;

        m_bufferUsage = initializer.bufferUsage;
        m_tableIndexCount = initializer.tableIndexCount;
        m_tableVertexCount = initializer.tableVertexCount;

        m_attributes = {};

        std::copy(initializer.attributes.begin(), initializer.attributes.end(), m_attributes.begin());

        return true;
    }

    void mesh_database::shutdown()
    {
        for (auto& table : m_tables)
        {
            table.meshes->shutdown(*m_allocator, *m_resourceManager);
        }

        m_tables.clear();
    }

    mesh_handle mesh_database::create_mesh(
        u32 meshAttributesMask, mesh_index_type indexType, u32 vertexCount, u32 indexCount)
    {
        if (vertexCount > m_tableVertexCount || indexCount > m_tableIndexCount)
        {
            return {};
        }

        const auto tableId = make_table_id(meshAttributesMask, indexType);

        auto it =
            std::find_if(m_tables.rbegin(), m_tables.rend(), [tableId](const table& t) { return t.id == tableId; });

        if (it == m_tables.rend())
        {
            auto& newTable = m_tables.emplace_back();
            newTable.id = tableId;
            newTable.nextMeshId = 1u;
            newTable.meshes = std::make_unique<mesh_table>();

            std::array<buffer_column_description, MaxAttributes> columnsBuffer;
            const std::span columns = attributes_to_buffer_columns(meshAttributesMask, m_attributes, columnsBuffer);

            const auto success = newTable.meshes->init(columns,
                *m_allocator,
                *m_resourceManager,
                m_bufferUsage,
                get_index_byte_size(indexType),
                m_tableVertexCount,
                m_tableIndexCount);

            if (!success)
            {
                m_tables.pop_back();
                return {};
            }

            it = m_tables.rbegin();
        }

        const u32 meshId = it->nextMeshId++;

        const mesh_table_entry meshEntry[] = {
            {.id = h32<string>{meshId}, .numVertices = vertexCount, .numIndices = indexCount}};

        if (!it->meshes->allocate_meshes(meshEntry))
        {
            return {};
        }

        return make_mesh_handle(u32(&*it - m_tables.data()), meshId);
    }

    bool mesh_database::fetch_buffers(
        mesh_handle mesh, std::span<const u32> attributes, std::span<buffer> vertexBuffers, buffer* indexBuffer) const
    {
        const auto [tableId, meshId] = parse_mesh_handle(mesh);

        std::array<h32<string>, MaxAttributes> namesBuffer;
        const std::span names = attributes_to_names(attributes, m_attributes, namesBuffer);

        return m_tables[tableId].meshes->fetch_buffers(*m_resourceManager,
            h32<string>{meshId},
            names,
            vertexBuffers,
            indexBuffer);
    }
}