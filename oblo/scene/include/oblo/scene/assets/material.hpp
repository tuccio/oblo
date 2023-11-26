#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/types.hpp>

#include <filesystem>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

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
        std::string_view name;
        material_property_type type;
        material_data_storage storage;

        template <typename T>
        expected<T, material_error> as() const;
    };

    template <typename T>
    constexpr material_property_type get_material_property_type();

    class SCENE_API material
    {
    public:
        material() = default;

        material(const material&) = delete;
        material(material&&) noexcept = default;

        material& operator=(const material&) = delete;
        material& operator=(material&&) noexcept = default;

        ~material() = default;

        void set_property(std::string_view name, material_property_type type, const material_data_storage& value);

        const material_property* get_property(const std::string_view name) const;

        std::span<const material_property> get_properties() const;

        template <typename T>
        void set_property(std::string_view name, const T& value)
        {
            material_data_storage storage;
            new (storage.buffer) T{value};
            set_property(std::move(name), get_material_property_type<T>(), storage);
        }

        bool save(const std::filesystem::path& destination) const;
        bool load(const std::filesystem::path& source);

    private:
        struct string_hash
        {
            using is_transparent = void;

            usize operator()(std::string_view txt) const
            {
                return std::hash<std::string_view>{}(txt);
            }

            usize operator()(const std::string& txt) const
            {
                return std::hash<std::string>{}(txt);
            }
        };

    private:
        std::unordered_map<std::string, usize, string_hash, std::equal_to<>> m_map;
        std::vector<material_property> m_properties;
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
