#include <oblo/scene/assets/mesh.hpp>

#include <oblo/core/unreachable.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/math/vec3.hpp>

#include <utility>

namespace oblo::scene
{
    namespace
    {
        constexpr std::pair<usize, usize> get_size_and_alignment(data_format format)
        {
            switch (format)
            {
            case data_format::i8:
                return {sizeof(i8), alignof(i8)};
            case data_format::i16:
                return {sizeof(i16), alignof(i16)};
            case data_format::i32:
                return {sizeof(i32), alignof(i32)};
            case data_format::i64:
                return {sizeof(i64), alignof(i64)};
            case data_format::u8:
                return {sizeof(u8), alignof(u8)};
            case data_format::u16:
                return {sizeof(u16), alignof(u16)};
            case data_format::u32:
                return {sizeof(u32), alignof(u32)};
            case data_format::u64:
                return {sizeof(u64), alignof(u64)};
            case data_format::f32:
                return {sizeof(f32), alignof(f32)};
            case data_format::f64:
                return {sizeof(f64), alignof(f64)};
            case data_format::vec2:
                return {sizeof(vec2), alignof(vec2)};
            case data_format::vec3:
                return {sizeof(vec3), alignof(vec3)};
            default:
                unreachable();
            }
        }
    }

    struct mesh::attribute_data : mesh_attribute
    {
        usize beginIndex;
        usize endIndex;
    };

    void mesh::allocate(primitive_kind primitive,
                        u32 numVertices,
                        u32 numIndices,
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
            hasIndices |= attribute.kind == attribute_kind::indices;

            const auto multiplier = attribute.kind == attribute_kind::indices ? numIndices : numVertices;
            const auto [size, alignment] = get_size_and_alignment(attribute.format);
            const auto padding = computePadding(alignment);

            const usize beginIndex = totalSize + padding;
            const usize endIndex = beginIndex + size * multiplier;

            m_attributes.emplace_back(attribute, beginIndex, endIndex);

            totalSize = endIndex;
            previousAlignment = alignment;
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

        m_storage.resize(totalSize);
    }

    mesh::mesh() = default;

    mesh::mesh(mesh&& other) noexcept = default;

    mesh& mesh::operator=(mesh&& other) noexcept = default;

    mesh::~mesh() = default;

    void mesh::clear()
    {
        *this = {};
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

    u32 mesh::get_elements_count(attribute_kind attribute) const
    {
        // TODO: Should check if the attribute is even there
        return attribute == attribute_kind::indices ? get_index_count() : get_vertex_count();
    }

    u32 mesh::get_attributes_count() const
    {
        return u32(m_attributes.size());
    }

    mesh_attribute mesh::get_attribute_at(u32 index) const
    {
        return m_attributes[index];
    }
}