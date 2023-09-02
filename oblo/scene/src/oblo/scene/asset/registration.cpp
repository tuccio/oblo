#include <oblo/asset/asset_registry.hpp>

#include <oblo/asset/asset_type_desc.hpp>
#include <oblo/scene/assets/bundle.hpp>
#include <oblo/scene/assets/mesh.hpp>
#include <oblo/scene/serialization/mesh_file.hpp>
#include <oblo/scene/assets/model.hpp>

#include <fstream>

#include <nlohmann/json.hpp>

namespace oblo::scene
{
    namespace
    {
        template <typename T>
        struct meta;

        template <>
        struct meta<scene::bundle>
        {
            static bool save(const scene::bundle& bundle, const std::filesystem::path& destination)
            {
                char uuidBuffer[36];

                auto meshes = nlohmann::json::array();

                for (const auto& mesh : bundle.meshes)
                {
                    meshes.emplace_back(mesh.id.format_to(uuidBuffer));
                }

                auto models = nlohmann::json::array();

                for (const auto& model : bundle.models)
                {
                    models.emplace_back(model.id.format_to(uuidBuffer));
                }

                nlohmann::ordered_json json;

                json["meshes"] = std::move(meshes);
                json["models"] = std::move(models);

                std::ofstream ofs{destination};

                if (!ofs)
                {
                    return false;
                }

                ofs << json.dump(1, '\t');
                return true;
            }

            static constexpr std::string_view extension{".obundle"};
        };

        template <>
        struct meta<scene::model>
        {
            static bool save(const scene::model& model, const std::filesystem::path& destination)
            {
                char uuidBuffer[36];

                auto meshes = nlohmann::json::array();

                for (const auto& mesh : model.meshes)
                {
                    meshes.emplace_back(mesh.id.format_to(uuidBuffer));
                }

                nlohmann::ordered_json json;

                json["meshes"] = std::move(meshes);

                std::ofstream ofs{destination};

                if (!ofs)
                {
                    return false;
                }

                ofs << json.dump(1, '\t');
                return true;
            }

            static constexpr std::string_view extension{".omodel"};
        };

        template <>
        struct meta<scene::mesh>
        {
            static bool save(const scene::mesh& mesh, const std::filesystem::path& destination)
            {
                save_mesh(mesh, destination);
                return true;
            }

            static constexpr std::string_view extension{".glb"};
        };
    }

    template <typename T>
    asset::asset_type_desc make_asset_type_desc()
    {
        return {
            .type = get_type_id<T>(),
            .create = []() -> void* { return new T{}; },
            .destroy = [](void* ptr) { delete static_cast<T*>(ptr); },
            .load =
                [](void*, const std::filesystem::path&)
            {
                // TODO
                return false;
            },
            .save = [](const void* ptr, const std::filesystem::path& destination)
            { return meta<T>::save(*static_cast<const T*>(ptr), destination); },
            .extension = meta<T>::extension,
        };
    }
    void register_asset_types(asset::asset_registry& registry)
    {
        registry.register_type(make_asset_type_desc<mesh>());
        registry.register_type(make_asset_type_desc<model>());
        registry.register_type(make_asset_type_desc<bundle>());
    }
}