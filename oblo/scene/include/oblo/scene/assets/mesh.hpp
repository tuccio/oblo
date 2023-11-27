#pragma once

#include <oblo/core/lifetime.hpp>
#include <oblo/core/types.hpp>

#include <span>
#include <vector>

namespace oblo
{
    enum class data_format : u8;

    enum class attribute_kind : u8
    {
        indices,
        position,
        normal,
        uv0,
        enum_max
    };

    enum class primitive_kind : u8
    {
        triangle,
        enum_max
    };

    struct mesh_attribute
    {
        attribute_kind kind;
        data_format format;
    };

    class SCENE_API mesh
    {
    public:
        mesh();
        mesh(const mesh&) = delete;
        mesh(mesh&& other) noexcept;
        mesh& operator=(const mesh&) = delete;
        mesh& operator=(mesh&& other) noexcept;
        ~mesh();

        void allocate(
            primitive_kind primitive, u32 vertexCount, u32 indexCount, std::span<const mesh_attribute> attributes);

        void clear();

        std::span<std::byte> get_attribute(attribute_kind kind, data_format* outFormat = nullptr);
        std::span<const std::byte> get_attribute(attribute_kind kind, data_format* outFormat = nullptr) const;

        template <typename T>
        std::span<T> get_attribute(attribute_kind attribute);

        template <typename T>
        std::span<const T> get_attribute(attribute_kind attribute) const;

        data_format get_attribute_format(attribute_kind attribute) const;

        u32 get_attributes_count() const;
        mesh_attribute get_attribute_at(u32 index) const;

        primitive_kind get_primitive_kind() const;
        u32 get_vertex_count() const;
        u32 get_index_count() const;
        u32 get_elements_count(attribute_kind attribute) const;

    private:
        template <typename T, typename Self>
        static auto get_attribute_impl(Self& self, attribute_kind attribute)
        {
            const std::span bytes = self.get_attribute(attribute);

            if (bytes.empty())
            {
                return decltype(std::span{start_lifetime_as_array<T>(bytes.data(), 0), 0}){};
            }

            OBLO_ASSERT(bytes.size() % sizeof(T) == 0);
            OBLO_ASSERT(bytes.size() % alignof(T) == 0);

            auto* const ptr = start_lifetime_as_array<T>(bytes.data(), bytes.size());
            const usize size = bytes.size() / sizeof(T);
            return std::span{ptr, size};
        };

    private:
        struct attribute_data;

    private:
        primitive_kind m_primitives{primitive_kind::enum_max};
        u32 m_vertexCount{};
        u32 m_indexCount{};
        std::vector<std::byte> m_storage;
        std::vector<attribute_data> m_attributes;
    };

    template <typename T>
    std::span<T> mesh::get_attribute(attribute_kind attribute)
    {
        return get_attribute_impl<T>(*this, attribute);
    }

    template <typename T>
    std::span<const T> mesh::get_attribute(attribute_kind attribute) const
    {
        return get_attribute_impl<T>(*this, attribute);
    }
}