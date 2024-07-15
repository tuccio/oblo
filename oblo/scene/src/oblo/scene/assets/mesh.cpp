#include <oblo/scene/assets/mesh.hpp>

#include <oblo/core/data_format.hpp>

#include <utility>

namespace oblo
{
    struct mesh::attribute_data : mesh_attribute
    {
        usize beginIndex;
        usize endIndex;
    };

    void mesh::allocate(primitive_kind primitive,
        u32 numVertices,
        u32 numIndices,
        u32 meshletCount,
        std::span<const mesh_attribute> attributes)
    {
        clear();

        bool hasIndices{false};

        m_attributes.reserve(attributes.size());

        // TODO: Check for duplicates?

        usize totalSize{0};
        usize previousAlignment{alignof(std::max_align_t)};

        const auto computePadding = [&previousAlignment](usize newAlignment)
        { return previousAlignment >= newAlignment ? usize(0) : newAlignment - previousAlignment; };

        for (const auto& attribute : attributes)
        {
            const auto isIndex = !is_vertex_attribute(attribute.kind);

            hasIndices |= isIndex;

            const auto multiplier = isIndex ? numIndices : numVertices;
            const auto [size, alignment] = get_size_and_alignment(attribute.format);
            const auto padding = computePadding(alignment);

            const usize beginIndex = totalSize + padding;
            const usize endIndex = beginIndex + size * multiplier;

            m_attributes.emplace_back(attribute, beginIndex, endIndex);

            totalSize = endIndex;
            previousAlignment = alignment;

            OBLO_ASSERT(!m_attributeFlags.contains(attribute.kind));
            m_attributeFlags |= attribute.kind;
        }

        if (!hasIndices)
        {
            m_indexCount = 0;
        }
        else
        {
            m_indexCount = numIndices;
        }

        m_vertexCount = numVertices;
        m_primitives = primitive;
        m_meshletCount = meshletCount;

        m_storage.resize(totalSize);
        m_meshlets.resize(meshletCount);
    }

    mesh::mesh() = default;

    mesh::mesh(mesh&& other) noexcept = default;

    mesh& mesh::operator=(mesh&& other) noexcept = default;

    mesh::~mesh() = default;

    void mesh::clear()
    {
        *this = {};
    }

    void mesh::reset_meshlets(u32 meshletCount)
    {
        m_meshletCount = meshletCount;
        m_meshlets.resize(meshletCount);
    }

    bool mesh::has_attribute(attribute_kind kind) const
    {
        return m_attributeFlags.contains(kind);
    }

    std::span<std::byte> mesh::get_attribute(attribute_kind kind, data_format* outFormat)
    {
        const std::span<const std::byte> data = static_cast<const mesh*>(this)->get_attribute(kind, outFormat);
        return {const_cast<std::byte*>(data.data()), data.size()};
    }

    std::span<const std::byte> mesh::get_attribute(attribute_kind kind, data_format* outFormat) const
    {
        for (const auto& attribute : m_attributes)
        {
            if (kind == attribute.kind)
            {
                if (outFormat)
                {
                    *outFormat = attribute.format;
                }

                return std::span{m_storage}.subspan(attribute.beginIndex, attribute.endIndex - attribute.beginIndex);
            }
        }

        return {};
    }

    primitive_kind mesh::get_primitive_kind() const
    {
        return m_primitives;
    }

    u32 mesh::get_vertex_count() const
    {
        return m_vertexCount;
    }

    u32 mesh::get_index_count() const
    {
        return m_indexCount;
    }

    u32 mesh::get_meshlet_count() const
    {
        return m_meshletCount;
    }

    u32 mesh::get_elements_count(attribute_kind attribute) const
    {
        if (!m_attributeFlags.contains(attribute))
        {
            return 0u;
        }

        return is_vertex_attribute(attribute) ? get_vertex_count() : get_index_count();
    }

    std::span<meshlet> mesh::get_meshlets()
    {
        return m_meshlets;
    }

    std::span<const meshlet> mesh::get_meshlets() const
    {
        return m_meshlets;
    }

    data_format mesh::get_attribute_format(attribute_kind kind) const
    {
        for (const auto& attribute : m_attributes)
        {
            if (attribute.kind == kind)
            {
                return attribute.format;
            }
        }

        return data_format::enum_max;
    }

    u32 mesh::get_attributes_count() const
    {
        return u32(m_attributes.size());
    }

    mesh_attribute mesh::get_attribute_at(u32 index) const
    {
        return m_attributes[index];
    }

    void mesh::update_aabb()
    {
        const auto positions = get_attribute<vec3>(attribute_kind::position);
        m_aabb = compute_aabb(positions);
    }

    aabb mesh::get_aabb() const
    {
        return m_aabb;
    }

    void mesh::set_aabb(aabb aabb)
    {
        m_aabb = aabb;
    }
}