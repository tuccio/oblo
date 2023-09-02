#include <oblo/asset/asset_registry.hpp>

#include <oblo/asset/any_asset.hpp>
#include <oblo/asset/asset_type_desc.hpp>
#include <oblo/asset/import_artifact.hpp>
#include <oblo/asset/import_preview.hpp>
#include <oblo/asset/importer.hpp>
#include <oblo/asset/meta.hpp>
#include <oblo/core/array_size.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/core/uuid_generator.hpp>

#include <nlohmann/json.hpp>

#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace oblo::asset
{
    namespace
    {
        struct asset_type_info : asset_type_desc
        {
        };

        struct file_importer_info
        {
            create_file_importer_fn create;
            std::vector<std::string> extensions;
        };

        bool save_asset_meta(const uuid& id, const asset_meta& meta, const std::filesystem::path& destination)
        {
            char uuidBuffer[36];

            nlohmann::ordered_json json;

            json["id"] = id.format_to(uuidBuffer);
            json["type"] = meta.type.name;

            if (!meta.importer.name.empty())
            {
                json["importer"] = meta.importer.name;
            }

            if (!meta.importId.is_nil())
            {
                json["importId"] = meta.importId.format_to(uuidBuffer);
            }

            if (!meta.importName.empty())
            {
                json["importName"] = meta.importName;
            }

            std::ofstream ofs{destination};

            if (!ofs)
            {
                return false;
            }

            ofs << json.dump(1, '\t');
            return !ofs.bad();
        }

        // TODO: Need to read the full meta instead
        bool load_asset_id_from_meta(const std::filesystem::path& path, uuid& id)
        {
            std::ifstream in{path};

            if (!in)
            {
                return false;
            }

            const auto json = nlohmann::json::parse(in, nullptr, false);
            const auto it = json.find("id");

            if (it == json.end())
            {
                return false;
            }

            return id.parse_from(it->get<std::string_view>());
        }

        bool save_artifacts_meta(std::span<const artifact_meta> artifacts, const std::filesystem::path& destination)
        {
            char uuidBuffer[36];

            nlohmann::json json;

            for (const auto& artifact : artifacts)
            {
                auto& artifactJson = json[artifact.id.format_to(uuidBuffer)];
                artifactJson["type"] = artifact.type.name;

                if (!artifact.name.empty())
                {
                    artifactJson["name"] = artifact.name;
                }
            }

            std::ofstream ofs{destination};

            if (!ofs)
            {
                return false;
            }

            ofs << json.dump(1, '\t');
            return !ofs.bad();
        }

        bool ensure_directories(const std::filesystem::path& directory)
        {
            std::error_code ec;
            std::filesystem::create_directories(directory, ec);
            return std::filesystem::is_directory(directory, ec);
        }

        constexpr std::string_view AssetMetaExtension{".oasset"};
        constexpr std::string_view ArtifactsMetaFilename{"artifacts.json"};
    }

    struct asset_registry::impl
    {
        uuid_random_generator uuidGenerator;
        std::unordered_map<type_id, asset_type_info> assetTypes;
        std::unordered_map<type_id, file_importer_info> importers;
        std::unordered_map<uuid, asset_meta> assets;
        std::filesystem::path assetsDir;
        std::filesystem::path artifactsDir;
    };

    asset_registry::asset_registry() = default;

    asset_registry::~asset_registry()
    {
        shutdown();
    }

    bool asset_registry::initialize(const std::filesystem::path& assetsDir, const std::filesystem::path& artifactsDir)
    {
        if (!ensure_directories(assetsDir) || !ensure_directories(artifactsDir))
        {
            return false;
        }

        m_impl = std::make_unique<impl>();
        m_impl->assetsDir = assetsDir;
        m_impl->artifactsDir = artifactsDir;

        return true;
    }

    void asset_registry::shutdown()
    {
        m_impl.reset();
    }

    void asset_registry::register_type(const asset_type_desc& desc)
    {
        m_impl->assetTypes.emplace(desc.type, desc);
    }

    bool asset_registry::has_asset_type(type_id type) const
    {
        return m_impl->assetTypes.contains(type);
    }

    bool asset_registry::create_directories(const std::filesystem::path& directory)
    {
        return ensure_directories(m_impl->assetsDir / directory);
    }

    void asset_registry::register_file_importer(const file_importer_desc& desc)
    {
        const auto [it, inserted] = m_impl->importers.emplace(desc.type, file_importer_info{});

        if (inserted)
        {
            auto& info = it->second;
            info.create = desc.create;
            info.extensions.reserve(desc.extensions.size());

            for (const std::string_view ext : desc.extensions)
            {
                info.extensions.emplace_back(ext);
            }
        }
    }

    importer asset_registry::create_importer(const std::filesystem::path& sourceFile)
    {
        const auto ext = sourceFile.extension();

        for (auto& [type, assetImporter] : m_impl->importers)
        {
            for (const auto& importerExt : assetImporter.extensions)
            {
                if (importerExt == ext)
                {
                    return importer{
                        importer_config{
                            .registry = this,
                            .sourceFile = sourceFile,
                            .fileImporterType = type,
                        },
                        assetImporter.create(),
                    };
                }
            }
        }

        return {};
    }

    uuid asset_registry::generate_uuid()
    {
        return m_impl->uuidGenerator.generate();
    }

    bool asset_registry::save_artifact(
        const uuid& importUuid, const uuid& artifactId, const type_id& type, const void* dataPtr, write_policy policy)
    {
        const auto typeIt = m_impl->assetTypes.find(type);

        if (typeIt == m_impl->assetTypes.end())
        {
            return false;
        }

        char uuidBuffer[36];

        auto artifactPath = m_impl->artifactsDir / importUuid.format_to(uuidBuffer);
        ensure_directories(artifactPath);

        artifactPath /= artifactId.format_to(uuidBuffer);

        std::error_code ec;

        if (policy == write_policy::no_overwrite && (std::filesystem::exists(artifactPath, ec) || ec))
        {
            return false;
        }

        const auto saveFunction = typeIt->second.save;
        return saveFunction(dataPtr, artifactPath);
    }

    bool asset_registry::save_asset(const std::filesystem::path& destination,
                                    std::string_view filename,
                                    asset_meta meta,
                                    write_policy policy)
    {
        const auto [assetIt, insertedAsset] = m_impl->assets.emplace(meta.id, std::move(meta));

        if (!insertedAsset)
        {
            return false;
        }

        auto fullPath = m_impl->assetsDir / destination / filename;
        fullPath.concat(AssetMetaExtension);

        std::error_code ec;

        if (policy == write_policy::no_overwrite && (std::filesystem::exists(fullPath, ec) || ec))
        {
            return false;
        }

        // TODO: Copy source files

        return save_asset_meta(meta.id, assetIt->second, fullPath);
    }

    bool asset_registry::save_artifacts_meta(const uuid& importId, std::span<const artifact_meta> artifacts)
    {
        char uuidBuffer[36];
        const auto artifactsDir = m_impl->artifactsDir / importId.format_to(uuidBuffer);
        return asset::save_artifacts_meta(artifacts, artifactsDir / ArtifactsMetaFilename);
    }

    bool asset_registry::find_asset_by_path(const std::filesystem::path& path, uuid& id, asset_meta& assetMeta) const
    {
        auto fullPath = m_impl->assetsDir / path;
        fullPath.concat(AssetMetaExtension);

        if (!load_asset_id_from_meta(fullPath, id))
        {
            return false;
        }

        const auto it = m_impl->assets.find(id);

        if (it == m_impl->assets.end())
        {
            return false;
        }

        assetMeta = it->second;
        return true;
    }
}