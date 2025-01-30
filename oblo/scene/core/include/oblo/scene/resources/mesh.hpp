#pragma once

#include <oblo/core/debug.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/flags.hpp>
#include <oblo/core/lifetime.hpp>
#include <oblo/core/types.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/math/aabb.hpp>
#include <oblo/scene/resources/traits.hpp>

#include <span>

namespace oblo
{
    enum class data_format : u8;

    enum class attribute_kind : u8
    {
        indices,
        microindices,
        position,
        normal,
        tangent,
        bitangent,
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

    struct meshlet
    {
        u32 vertexOffset;
        u32 vertexCount;
        u32 indexOffset;
        u32 indexCount;
    };

    class mesh
    {
    public:
        SCENE_API mesh();
        mesh(const mesh&) = delete;
        SCENE_API mesh(mesh&& other) noexcept;
        mesh& operator=(const mesh&) = delete;
        SCENE_API mesh& operator=(mesh&& other) noexcept;
        SCENE_API ~mesh();

        SCENE_API void allocate(primitive_kind primitive,
            u32 vertexCount,
            u32 indexCount,
            u32 meshletCount,
            std::span<const mesh_attribute> attributes);

        SCENE_API void clear();

        SCENE_API void reset_meshlets(u32 numMeshlets);

        SCENE_API bool has_attribute(attribute_kind kind) const;

        SCENE_API std::span<std::byte> get_attribute(attribute_kind kind, data_format* outFormat = nullptr);
        SCENE_API std::span<const std::byte> get_attribute(attribute_kind kind, data_format* outFormat = nullptr) const;

        template <typename T>
        std::span<T> get_attribute(attribute_kind attribute);

        template <typename T>
        std::span<const T> get_attribute(attribute_kind attribute) const;

        SCENE_API data_format get_attribute_format(attribute_kind attribute) const;

        SCENE_API u32 get_attributes_count() const;
        SCENE_API mesh_attribute get_attribute_at(u32 index) const;

        SCENE_API primitive_kind get_primitive_kind() const;
        SCENE_API u32 get_vertex_count() const;
        SCENE_API u32 get_index_count() const;
        SCENE_API u32 get_meshlet_count() const;
        SCENE_API u32 get_elements_count(attribute_kind attribute) const;

        SCENE_API std::span<meshlet> get_meshlets();
        SCENE_API std::span<const meshlet> get_meshlets() const;

        SCENE_API void update_aabb();
        SCENE_API aabb get_aabb() const;
        SCENE_API void set_aabb(aabb aabb);

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
        u32 m_meshletCount{};
        flags<attribute_kind> m_attributeFlags{};
        dynamic_array<std::byte> m_storage;
        dynamic_array<attribute_data> m_attributes;
        dynamic_array<meshlet> m_meshlets;
        aabb m_aabb{aabb::make_invalid()};
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

    constexpr bool is_vertex_attribute(attribute_kind kind)
    {
        return kind >= attribute_kind::position;
    }
}