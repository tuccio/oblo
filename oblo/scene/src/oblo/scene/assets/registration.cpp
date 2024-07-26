#include <oblo/scene/assets/registration.hpp>

#include <oblo/resource/resource_registry.hpp>
#include <oblo/resource/type_desc.hpp>
#include <oblo/scene/assets/material.hpp>
#include <oblo/scene/assets/mesh.hpp>
#include <oblo/scene/assets/model.hpp>
#include <oblo/scene/assets/texture.hpp>
#include <oblo/scene/serialization/mesh_file.hpp>

#include <fstream>

#include <nlohmann/json.hpp>

namespace oblo
{
    template <typename Json>
    void to_json(Json& json, const string_view& value)
    {
        json = std::string_view{value.data(), value.size()};
    }

    template <typename Json>
    void from_json(const Json& json, string_view& value)
    {
        const auto sv = json.template get<std::string_view>();
        value = string_view{sv.data(), sv.size()};
    }

    template <typename Json, typename T>
    void to_json(Json& json, const resource_ref<T>& value)
    {
        char uuidBuffer[36];
        json = value.id.format_to(uuidBuffer);
    }

    template <typename Json, typename T>
    void from_json(const Json& json, resource_ref<T>& value)
    {
        const auto res = uuid::parse(json.template get<string_view>());
        value.id = res ? *res : uuid{};
    }
}

namespace oblo
{
    namespace
    {
        template <typename T>
        struct meta;

        template <>
        struct meta<model>
        {
            static bool save(const model& model, cstring_view destination)
            {
                nlohmann::ordered_json json;

                json["meshes"] = model.meshes;
                json["materials"] = model.materials;

                std::ofstream ofs{destination.as<std::string>()};

                if (!ofs)
                {
                    return false;
                }

                ofs << json.dump(1, '\t');
                return true;
            }

            static bool load(model& model, cstring_view source)
            {
                try
                {
                    std::ifstream ifs{source.as<std::string>()};
                    const auto json = nlohmann::json::parse(ifs);
                    json.at("meshes").get_to(model.meshes);
                    json.at("materials").get_to(model.materials);
                    return true;
                }
                catch (const std::exception&)
                {
                    return false;
                }
            }
        };

        template <>
        struct meta<mesh>
        {
            static bool save(const mesh& mesh, cstring_view destination)
            {
                return save_mesh(mesh, destination);
            }

            static bool load(mesh& mesh, cstring_view source)
            {
                return load_mesh(mesh, source);
            }
        };

        template <>
        struct meta<texture>
        {
            static bool save(const texture& texture, cstring_view destination)
            {
                return texture.save(destination);
            }

            static bool load(texture& texture, cstring_view source)
            {
                return texture.load(source);
            }
        };

        template <>
        struct meta<material>
        {
            static bool save(const material& material, cstring_view destination)
            {
                return material.save(destination);
            }

            static bool load(material& material, cstring_view source)
            {
                return material.load(source);
            }
        };

        template <typename T>
        resource_type_desc make_resource_type_desc()
        {
            return {
                .type = get_type_id<T>(),
                .create = []() -> void* { return new T{}; },
                .destroy = [](void* ptr) { delete static_cast<T*>(ptr); },
                .load = [](void* ptr, cstring_view source) { return meta<T>::load(*static_cast<T*>(ptr), source); },
                .save = [](const void* ptr, cstring_view destination)
                { return meta<T>::save(*static_cast<const T*>(ptr), destination); },
            };
        }
    }

    void fetch_scene_resource_types(dynamic_array<resource_type_desc>& outResourceTypes)
    {
        outResourceTypes.push_back(make_resource_type_desc<material>());
        outResourceTypes.push_back(make_resource_type_desc<mesh>());
        outResourceTypes.push_back(make_resource_type_desc<model>());
        outResourceTypes.push_back(make_resource_type_desc<texture>());
    }
}