#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/expected.hpp>
#include <oblo/core/flags.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/string/hashed_string_view.hpp>
#include <oblo/core/string/string.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/types.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/reflection/codegen/annotations.hpp>
#include <oblo/scene/resources/traits.hpp>

#include <span>
#include <unordered_map>

namespace oblo
{
    enum class material_type_tag
    {
        none,
        linear_color,
    };

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
        linear_color_rgb_f32,
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

        OBLO_SCENE_API type_id get_property_type_id() const;
    };

    template <typename T, material_type_tag Tag>
    material_property_type get_material_property_type();

    class material
    {
    public:
        material() = default;

        material(const material&) = delete;
        material(material&&) noexcept = default;

        material& operator=(const material&) = delete;
        material& operator=(material&&) noexcept = default;

        ~material() = default;

        OBLO_SCENE_API void set_property(
            hashed_string_view name, material_property_type type, const material_data_storage& value);

        OBLO_SCENE_API const material_property* get_property(hashed_string_view name) const;

        OBLO_SCENE_API std::span<const material_property> get_properties() const;

        template <material_type_tag Tag, typename T>
        void set_property(hashed_string_view name, const T& value)
        {
            material_data_storage storage;
            new (storage.buffer) T{value};
            set_property(name, get_material_property_type<T, Tag>(), storage);
        }

        template <typename T>
        void set_property(hashed_string_view name, const T& value)
        {
            set_property<material_type_tag::none, T>(name, value);
        }

        OBLO_SCENE_API expected<> save(cstring_view destination) const;
        OBLO_SCENE_API expected<> load(cstring_view source);

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
    } OBLO_RESOURCE();

    template <typename T>
    expected<T, material_error> material_property::as() const
    {
        if (get_property_type_id() != get_type_id<T>())
        {
            return material_error::type_mismatch;
        }

        return *reinterpret_cast<const T*>(storage.buffer);
    }

    template <typename T>
        requires std::is_trivially_copyable_v<T>
    material_data_storage make_material_data_storage(const T& value)
    {
        material_data_storage storage;
        new (storage.buffer) T{value};
        return storage;
    }
}
