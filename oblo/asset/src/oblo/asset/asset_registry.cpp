#include <oblo/asset/asset_registry.hpp>

#include <oblo/asset/any_asset.hpp>
#include <oblo/asset/asset_meta.hpp>
#include <oblo/asset/asset_type_desc.hpp>
#include <oblo/asset/import_artifact.hpp>
#include <oblo/asset/import_preview.hpp>
#include <oblo/asset/importer.hpp>
#include <oblo/core/array_size.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/log.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/core/uuid_generator.hpp>

#include <nlohmann/json.hpp>

#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace oblo
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

        using asset_types_map = std::unordered_map<type_id, asset_type_info>;

        bool load_asset_meta(asset_meta& meta,
            std::vector<uuid>& artifacts,
            const asset_types_map& assetTypes,
            const std::filesystem::path& source)
        {
            std::ifstream in{source};

            if (!in)
            {
                return false;
            }

            const auto json = nlohmann::json::parse(in, nullptr, false);

            if (json.is_discarded())
            {
                return false;
            }

            const std::string_view id = json["id"].get<std::string_view>();

            if (auto parsed = uuid::parse(id))
            {
                meta.id = *parsed;
            }
            else
            {
                return false;
            }

            const hashed_string_view type{json["typeHint"].get<std::string_view>()};

            const auto typeIt = assetTypes.find(type_id{type});

            if (typeIt != assetTypes.end())
            {
                meta.typeHint = typeIt->first;
            }

            const std::string_view mainArtifactHint = json["mainArtifactHint"].get<std::string_view>();

            if (auto parsed = uuid::parse(mainArtifactHint))
            {
                meta.mainArtifactHint = *parsed;
            }
            else
            {
                return false;
            }

            const auto isImported = json.find("isImported");

            if (isImported != json.end())
            {
                meta.isImported = isImported->get<bool>();
            }

            const auto artifactsJson = json.find("artifacts");

            if (artifactsJson != json.end())
            {
                for (auto& artifact : *artifactsJson)
                {
                    if (const auto parsed = uuid::parse(artifact.get<std::string_view>()))
                    {
                        artifacts.emplace_back(*parsed);
                    }
                }
            }

            return true;
        }

        bool save_asset_meta(
            const asset_meta& meta, std::span<const uuid> artifacts, const std::filesystem::path& destination)
        {
            char uuidBuffer[36];

            nlohmann::ordered_json json;

            json["id"] = meta.id.format_to(uuidBuffer);
            json["mainArtifactHint"] = meta.mainArtifactHint.format_to(uuidBuffer);
            json["typeHint"] = meta.typeHint.name.as<std::string_view>();
            json["isImported"] = meta.isImported;

            auto&& artifactsJson = json["artifacts"];

            for (const auto& uuid : artifacts)
            {
                artifactsJson.push_back(uuid.format_to(uuidBuffer));
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

            if (json.is_discarded())
            {
                return false;
            }

            const auto it = json.find("id");

            if (it == json.end())
            {
                return false;
            }

            return id.parse_from(it->get<std::string_view>());
        }

        bool save_artifact_meta(const artifact_meta& artifact, const std::filesystem::path& destination)
        {
            char uuidBuffer[36];

            nlohmann::json json;

            json["id"] = artifact.id.format_to(uuidBuffer);
            json["type"] = artifact.type.name.as<std::string_view>();

            if (!artifact.importId.is_nil())
            {
                json["importId"] = artifact.importId.format_to(uuidBuffer);
            }

            if (!artifact.importName.empty())
            {
                json["name"] = artifact.importName;
            }

            std::ofstream ofs{destination};

            if (!ofs)
            {
                return false;
            }

            ofs << json.dump(1, '\t');
            return !ofs.bad();
        }

        bool load_artifact_meta(const std::filesystem::path& source,
            const std::unordered_map<type_id, asset_type_info>& assetTypes,
            artifact_meta& artifact)
        {
            std::ifstream ifs{source};

            if (!ifs)
            {
                return false;
            }

            const auto json = nlohmann::json::parse(ifs, nullptr, false);

            if (json.empty())
            {
                return false;
            }

            if (const auto it = json.find("id"); it != json.end())
            {
                const auto id = uuid::parse(it->get<std::string_view>());
                artifact.id = id ? *id : uuid{};
            }

            if (const auto it = json.find("type"); it != json.end())
            {
                const auto type = hashed_string_view{it->get<std::string_view>()};

                if (const auto typeIt = assetTypes.find(type_id{type}); typeIt == assetTypes.end())
                {
                    return false;
                }
                else
                {
                    artifact.type = typeIt->first;
                }
            }

            if (const auto it = json.find("name"); it != json.end())
            {
                artifact.importName = it->get<std::string>();
            }

            if (const auto it = json.find("importId"); it != json.end())
            {
                const auto id = uuid::parse(it->get<std::string_view>());
                artifact.id = id ? *id : uuid{};
            }

            return true;
        }

        bool ensure_directories(const std::filesystem::path& directory)
        {
            std::error_code ec;
            std::filesystem::create_directories(directory, ec);
            return std::filesystem::is_directory(directory, ec);
        }

        struct asset_entry
        {
            asset_meta meta;
            std::vector<uuid> artifacts;
        };

        constexpr std::string_view ArtifactMetaExtension{".oartifact"};
    }

    struct asset_registry::impl
    {
        uuid_random_generator uuidGenerator;
        std::unordered_map<type_id, asset_type_info> assetTypes;
        std::unordered_map<type_id, file_importer_info> importers;
        std::unordered_map<uuid, asset_entry> assets;
        std::filesystem::path assetsDir;
        std::filesystem::path artifactsDir;
        std::filesystem::path sourceFilesDir;
    };

    asset_registry::asset_registry() = default;

    asset_registry::~asset_registry()
    {
        shutdown();
    }

    bool asset_registry::initialize(const std::filesystem::path& assetsDir,
        const std::filesystem::path& artifactsDir,
        const std::filesystem::path& sourceFilesDir)
    {
        if (!ensure_directories(assetsDir) || !ensure_directories(artifactsDir) || !ensure_directories(sourceFilesDir))
        {
            return false;
        }

        std::error_code ec;

        m_impl = std::make_unique<impl>();

        if (m_impl->assetsDir = std::filesystem::absolute(assetsDir, ec); ec)
        {
            return false;
        }

        if (m_impl->artifactsDir = std::filesystem::absolute(artifactsDir, ec); ec)
        {
            return false;
        }

        if (m_impl->sourceFilesDir = std::filesystem::absolute(sourceFilesDir, ec); ec)
        {
            return false;
        }

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

    void asset_registry::unregister_type(type_id type)
    {
        m_impl->assetTypes.erase(type);
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

    void asset_registry::unregister_file_importer(type_id type)
    {
        m_impl->importers.erase(type);
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
                        },
                        type,
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

    bool asset_registry::save_artifact(const uuid& artifactId,
        const type_id& type,
        const void* dataPtr,
        const artifact_meta& meta,
        write_policy policy)
    {
        const auto typeIt = m_impl->assetTypes.find(type);

        if (typeIt == m_impl->assetTypes.end())
        {
            return false;
        }

        char uuidBuffer[36];

        auto artifactPath = m_impl->artifactsDir;
        ensure_directories(artifactPath);

        artifactPath /= artifactId.format_to(uuidBuffer);

        std::error_code ec;

        if (policy == write_policy::no_overwrite && (std::filesystem::exists(artifactPath, ec) || ec))
        {
            return false;
        }

        auto artifactMetaPath = artifactPath;
        artifactMetaPath.concat(ArtifactMetaExtension);

        const auto saveFunction = typeIt->second.save;
        return saveFunction(dataPtr, artifactPath) && save_artifact_meta(meta, artifactMetaPath);
    }

    bool asset_registry::save_asset(const std::filesystem::path& destination,
        const std::filesystem::path& fileName,
        const asset_meta& meta,
        std::span<const uuid> artifacts,
        write_policy policy)
    {
        const auto [assetIt, insertedAsset] = m_impl->assets.emplace(meta.id,
            asset_entry{std::move(meta), std::vector<uuid>{artifacts.begin(), artifacts.end()}});

        if (!insertedAsset)
        {
            return false;
        }

        auto fullPath = m_impl->assetsDir / destination / fileName;
        fullPath.concat(AssetMetaExtension);

        std::error_code ec;

        if (policy == write_policy::no_overwrite && (std::filesystem::exists(fullPath, ec) || ec))
        {
            return false;
        }

        return save_asset_meta(assetIt->second.meta, artifacts, fullPath);
    }

    std::filesystem::path asset_registry::create_source_files_dir(uuid importId)
    {
        char uuidBuffer[36];
        auto importDir = m_impl->sourceFilesDir / importId.format_to(uuidBuffer);

        if (!ensure_directories(importDir))
        {
            return {};
        }

        return importDir;
    }

    bool asset_registry::find_asset_by_id(const uuid& id, asset_meta& assetMeta) const
    {
        const auto it = m_impl->assets.find(id);

        if (it == m_impl->assets.end())
        {
            return false;
        }

        assetMeta = it->second.meta;
        return true;
    }

    bool asset_registry::find_asset_by_path(const std::filesystem::path& path, uuid& id, asset_meta& assetMeta) const
    {
        auto fullPath = m_impl->assetsDir / path;
        fullPath.concat(AssetMetaExtension);

        return find_asset_by_meta_path(fullPath, id, assetMeta);
    }

    bool asset_registry::find_asset_by_meta_path(
        const std::filesystem::path& path, uuid& id, asset_meta& assetMeta) const
    {
        if (!load_asset_id_from_meta(path, id))
        {
            return false;
        }

        return find_asset_by_id(id, assetMeta);
    }

    bool asset_registry::find_asset_artifacts(const uuid& id, dynamic_array<uuid>& artifacts) const
    {
        const auto it = m_impl->assets.find(id);

        if (it == m_impl->assets.end())
        {
            return false;
        }

        artifacts.assign(it->second.artifacts.begin(), it->second.artifacts.end());
        return true;
    }

    bool asset_registry::load_artifact_meta(const uuid& id, artifact_meta& artifact) const
    {
        char uuidBuffer[36];

        const auto& artifactsDir = m_impl->artifactsDir;

        auto resourceFile = artifactsDir / id.format_to(uuidBuffer);

        if (std::error_code ec; !std::filesystem::exists(resourceFile, ec))
        {
            return false;
        }

        auto resourceMeta = resourceFile;
        resourceMeta.concat(ArtifactMetaExtension);

        return oblo::load_artifact_meta(resourceMeta, m_impl->assetTypes, artifact);
    }

    const std::filesystem::path& asset_registry::get_asset_directory() const
    {
        return m_impl->assetsDir;
    }

    bool asset_registry::find_artifact_resource(
        const uuid& id, type_id& outType, std::string& outName, std::filesystem::path& outPath, const void* userdata)
    {
        char uuidBuffer[36];

        auto* const self = static_cast<const asset_registry*>(userdata);
        const auto& artifactsDir = self->m_impl->artifactsDir;

        auto resourceFile = artifactsDir / id.format_to(uuidBuffer);

        if (std::error_code ec; !std::filesystem::exists(resourceFile, ec))
        {
            return false;
        }

        auto resourceMeta = resourceFile;
        resourceMeta.concat(ArtifactMetaExtension);

        artifact_meta meta;

        if (!oblo::load_artifact_meta(resourceMeta, self->m_impl->assetTypes, meta))
        {
            return false;
        }

        outType = meta.type;
        outPath = std::move(resourceFile);
        outName = std::move(meta.importName);

        return true;
    }

    void asset_registry::discover_assets()
    {
        std::error_code ec;

        for (auto&& entry : std::filesystem::recursive_directory_iterator{m_impl->assetsDir, ec})
        {
            const auto& p = entry.path();

            asset_meta meta{};
            std::vector<uuid> artifacts;

            if (load_asset_meta(meta, artifacts, m_impl->assetTypes, p))
            {
                m_impl->assets.emplace(meta.id, asset_entry{meta, std::move(artifacts)});
            }
            else
            {
                log::warn("Failed to load asset meta {}", p.string());
            }
        }
    }
}