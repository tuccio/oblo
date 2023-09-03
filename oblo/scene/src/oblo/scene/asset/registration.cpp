#include <oblo/asset/asset_registry.hpp>

#include <oblo/asset/asset_type_desc.hpp>
#include <oblo/resource/resource_registry.hpp>
#include <oblo/scene/assets/mesh.hpp>
#include <oblo/scene/assets/model.hpp>
#include <oblo/scene/serialization/mesh_file.hpp>

#include <fstream>

#include <nlohmann/json.hpp>

namespace oblo::asset
{
    template <typename Json, typename T>
    void to_json(Json& json, const ref<T>& value)
    {
        char uuidBuffer[36];
        json = value.id.format_to(uuidBuffer);
    }

    template <typename Json, typename T>
    void from_json(const Json& json, ref<T>& value)
    {
        const auto res = uuid::parse(json.template get<std::string_view>());
        value.id = res ? *res : uuid{};
    }
}

namespace oblo::scene
{
    namespace
    {
        template <typename T>
        struct meta;

        template <>
        struct meta<scene::model>
        {
            static bool save(const scene::model& model, const std::filesystem::path& destination)
            {
                nlohmann::ordered_json json;

                json["meshes"] = model.meshes;

                std::ofstream ofs{destination};

                if (!ofs)
                {
                    return false;
                }

                ofs << json.dump(1, '\t');
                return true;
            }

            static bool load(scene::model& model, const std::filesystem::path& source)
            {
                try
                {
                    std::ifstream ifs{source};
                    const auto json = nlohmann::json::parse(ifs);
                    json.at("meshes").get_to(model.meshes);
                    return true;
                }
                catch (const std::exception&)
                {
                    return false;
                }
            }
        };

        template <>
        struct meta<scene::mesh>
        {
            static bool save(const scene::mesh& mesh, const std::filesystem::path& destination)
            {
                return save_mesh(mesh, destination);
            }

            static bool load(scene::mesh& mesh, const std::filesystem::path& source)
            {
                return load_mesh(mesh, source);
            }
        };
    }

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

    template <typename T>
    asset::asset_type_desc make_asset_type_desc()
    {
        return {make_resource_type_desc<T>()};
    }

    void register_asset_types(asset::asset_registry& registry)
    {
        registry.register_type(make_asset_type_desc<mesh>());
        registry.register_type(make_asset_type_desc<model>());
    }

    void register_resource_types(resource_registry& registry)
    {
        registry.register_type(make_resource_type_desc<mesh>());
        registry.register_type(make_resource_type_desc<model>());
    }
}