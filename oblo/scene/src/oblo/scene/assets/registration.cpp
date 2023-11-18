#include <oblo/scene/assets/registration.hpp>

#include <oblo/asset/asset_registry.hpp>

#include <oblo/asset/asset_type_desc.hpp>
#include <oblo/engine/engine_module.hpp>
#include <oblo/resource/resource_registry.hpp>
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
        json = value.id.format_to(uuidBuffer);
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
                return material.save(engine_module::get().get_property_registry(), destination);
            }

            static bool load(material& material, const std::filesystem::path& source)
            {
                return material.load(engine_module::get().get_property_registry(), source);
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
    asset_type_desc make_asset_type_desc()
    {
        return {make_resource_type_desc<T>()};
    }

    void register_asset_types(asset_registry& registry)
    {
        registry.register_type(make_asset_type_desc<material>());
        registry.register_type(make_asset_type_desc<mesh>());
        registry.register_type(make_asset_type_desc<model>());
        registry.register_type(make_asset_type_desc<texture>());
    }

    void unregister_asset_types(asset_registry& registry)
    {
        registry.unregister_type(get_type_id<material>());
        registry.unregister_type(get_type_id<mesh>());
        registry.unregister_type(get_type_id<model>());
        registry.unregister_type(get_type_id<texture>());
    }

    void register_resource_types(resource_registry& registry)
    {
        registry.register_type(make_resource_type_desc<mesh>());
        registry.register_type(make_resource_type_desc<model>());
        registry.register_type(make_resource_type_desc<texture>());
    }

    void unregister_resource_types(resource_registry& registry)
    {
        registry.unregister_type(get_type_id<mesh>());
        registry.unregister_type(get_type_id<model>());
        registry.unregister_type(get_type_id<texture>());
    }
}