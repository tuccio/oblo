#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/expected.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/string/hashed_string_view.hpp>
#include <oblo/core/string/string.hpp>
#include <oblo/core/types.hpp>
#include <oblo/core/uuid.hpp>

#include <span>
#include <unordered_map>

namespace oblo
{
    class property_registry;
    class texture;

    struct vec2;
    struct vec3;
    struct vec4;

    template <typename T>
    struct resource_ref;

    struct material_data_storage
    {
        alignas(long double) std::byte buffer[16];
    };

    enum class material_property_type : u8
    {
        f32,
        vec2,
        vec3,
        vec4,
        texture,
    };

    enum class material_error : u8
    {
        type_mismatch,
    };

    struct material_property
    {
        hashed_string_view name;
        material_property_type type;
        material_data_storage storage;

        template <typename T>
        expected<T, material_error> as() const;
    };

    template <typename T>
    constexpr material_property_type get_material_property_type();

    class material
    {
    public:
        material() = default;

        material(const material&) = delete;
        material(material&&) noexcept = default;

        material& operator=(const material&) = delete;
        material& operator=(material&&) noexcept = default;

        ~material() = default;

        SCENE_API void set_property(
            hashed_string_view name, material_property_type type, const material_data_storage& value);

        SCENE_API const material_property* get_property(hashed_string_view name) const;

        SCENE_API std::span<const material_property> get_properties() const;

        template <typename T>
        void set_property(hashed_string_view name, const T& value)
        {
            material_data_storage storage;
            new (storage.buffer) T{value};
            set_property(name, get_material_property_type<T>(), storage);
        }

        SCENE_API bool save(cstring_view destination) const;
        SCENE_API bool load(cstring_view source);

    private:
        struct string_hash
        {
            using is_transparent = void;

            usize operator()(hashed_string_view txt) const
            {
                return txt.hash();
            }

            usize operator()(const string& txt) const
            {
                return hashed_string_view{txt}.hash();
            }
        };

        struct string_equal
        {
            using is_transparent = void;

            bool operator()(const string& lhs, const hashed_string_view& rhs) const
            {
                return string_view{lhs} == rhs;
            }

            bool operator()(const hashed_string_view& lhs, const string& rhs) const
            {
                return this->operator()(rhs, lhs);
            }

            bool operator()(const string& lhs, const string& rhs) const
            {
                return lhs == rhs;
            }
        };

    private:
        std::unordered_map<string, usize, string_hash, string_equal> m_map;
        dynamic_array<material_property> m_properties;
    };

    template <typename T>
    expected<T, material_error> material_property::as() const
    {
        if (type != get_material_property_type<T>())
        {
            return material_error::type_mismatch;
        }

        return *reinterpret_cast<const T*>(storage.buffer);
    }

    template <typename T>
    constexpr material_property_type get_material_property_type();

    template <>
    constexpr material_property_type get_material_property_type<f32>()
    {
        return material_property_type::f32;
    }

    template <>
    constexpr material_property_type get_material_property_type<vec2>()
    {
        return material_property_type::vec2;
    }

    template <>
    constexpr material_property_type get_material_property_type<vec3>()
    {
        return material_property_type::vec3;
    }

    template <>
    constexpr material_property_type get_material_property_type<vec4>()
    {
        return material_property_type::vec4;
    }

    template <>
    constexpr material_property_type get_material_property_type<resource_ref<texture>>()
    {
        return material_property_type::texture;
    }
}
