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
    template <typename Json, typename T>
    void to_json(Json& json, const resource_ref<T>& value)
    {
        char uuidBuffer[36];
        json = value.id.format_to(uuidBuffer).as<std::string_view>();
    }

    template <typename Json, typename T>
    void from_json(const Json& json, resource_ref<T>& value)
    {
        const auto res = uuid::parse(json.template get<std::string_view>());
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
            static bool save(const model& model, const std::filesystem::path& destination)
            {
                nlohmann::ordered_json json;

                json["meshes"] = model.meshes;
                json["materials"] = model.materials;

                std::ofstream ofs{destination};

                if (!ofs)
                {
                    return false;
                }

                ofs << json.dump(1, '\t');
                return true;
            }

            static bool load(model& model, const std::filesystem::path& source)
            {
                try
                {
                    std::ifstream ifs{source};
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
            static bool save(const mesh& mesh, const std::filesystem::path& destination)
            {
                return save_mesh(mesh, destination);
            }

            static bool load(mesh& mesh, const std::filesystem::path& source)
            {
                return load_mesh(mesh, source);
            }
        };

        template <>
        struct meta<texture>
        {
            static bool save(const texture& texture, const std::filesystem::path& destination)
            {
                return texture.save(destination);
            }

            static bool load(texture& texture, const std::filesystem::path& source)
            {
                return texture.load(source);
            }
        };

        template <>
        struct meta<material>
        {
            static bool save(const material& material, const std::filesystem::path& destination)
            {
                return material.save(destination);
            }

            static bool load(material& material, const std::filesystem::path& source)
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
                .load = [](void* ptr, const std::filesystem::path& source)
                { return meta<T>::load(*static_cast<T*>(ptr), source); },
                .save = [](const void* ptr, const std::filesystem::path& destination)
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