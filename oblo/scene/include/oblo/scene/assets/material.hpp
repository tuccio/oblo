#pragma once

#include <oblo/core/type_id.hpp>
#include <oblo/core/types.hpp>

#include <filesystem>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace oblo
{
    class property_registry;

    struct material_data_storage
    {
        alignas(long double) std::byte buffer[16];
    };

    struct material_property
    {
        std::string_view name;
        type_id type;
        material_data_storage storage;
    };

    class SCENE_API material
    {
    public:
        material() = default;

        material(const material&) = delete;
        material(material&&) noexcept = default;

        material& operator=(const material&) = delete;
        material& operator=(material&&) noexcept = default;

        ~material() = default;

        void set_property(std::string name, const type_id& type, const material_data_storage& value);

        const material_property* get_property(const std::string_view name) const;

        std::span<const material_property> get_properties() const;

        template <typename T>
        void set_property(std::string name, const T& value)
        {
            material_data_storage storage;
            new (storage.buffer) T{value};
            set_property(std::move(name), get_type_id<T>(), storage);
        }

        bool save(const property_registry& registry, const std::filesystem::path& destination) const;
        bool load(const property_registry& registry, const std::filesystem::path& source);

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
}
