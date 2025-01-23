#include <oblo/asset/asset_registry.hpp>

#include <oblo/asset/asset_meta.hpp>
#include <oblo/asset/import/import_artifact.hpp>
#include <oblo/asset/import/import_preview.hpp>
#include <oblo/asset/import/importer.hpp>
#include <oblo/core/array_size.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/filesystem/filesystem.hpp>
#include <oblo/core/invoke/function_ref.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/core/uuid_generator.hpp>
#include <oblo/log/log.hpp>

#include <nlohmann/json.hpp>

#include <fstream>
#include <unordered_map>
#include <vector>

namespace oblo
{
    namespace
    {
        struct file_importer_info
        {
            create_file_importer_fn create;
            dynamic_array<string> extensions;
        };

        bool load_asset_meta(asset_meta& meta, std::vector<uuid>& artifacts, const std::filesystem::path& path)
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

            const std::string_view id = json["id"].get<std::string_view>();

            if (auto parsed = uuid::parse(id))
            {
                meta.id = *parsed;
            }
            else
            {
                return false;
            }

            const std::string_view type = json["typeHint"].get<std::string_view>();

            if (auto parsed = uuid::parse(type))
            {
                meta.typeHint = *parsed;
            }
            else
            {
                return false;
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

        bool save_asset_meta(const asset_meta& meta, const deque<uuid>& artifacts, cstring_view destination)
        {
            char uuidBuffer[36];

            nlohmann::ordered_json json;

            json["id"] = meta.id.format_to(uuidBuffer).as<std::string_view>();
            json["mainArtifactHint"] = meta.mainArtifactHint.format_to(uuidBuffer).as<std::string_view>();
            json["typeHint"] = meta.typeHint.format_to(uuidBuffer).as<std::string_view>();
            json["isImported"] = meta.isImported;

            auto&& artifactsJson = json["artifacts"];

            for (const auto& uuid : artifacts)
            {
                artifactsJson.push_back(uuid.format_to(uuidBuffer).as<std::string_view>());
            }

            std::ofstream ofs{destination.as<std::string>()};

            if (!ofs)
            {
                return false;
            }

            ofs << json.dump(1, '\t');
            return !ofs.bad();
        }

        // TODO: Need to read the full meta instead
        bool load_asset_id_from_meta(string_view path, uuid& id)
        {
            std::ifstream in{path.as<std::string>()};

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

        bool save_artifact_meta(const artifact_meta& artifact, cstring_view destination)
        {
            char uuidBuffer[36];

            nlohmann::json json;

            json["id"] = artifact.id.format_to(uuidBuffer).as<std::string_view>();
            json["type"] = artifact.type.format_to(uuidBuffer).as<std::string_view>();

            if (!artifact.importId.is_nil())
            {
                json["importId"] = artifact.importId.format_to(uuidBuffer).as<std::string_view>();
            }

            if (!artifact.importName.empty())
            {
                json["name"] = artifact.importName.as<std::string>();
            }

            std::ofstream ofs{destination.as<std::string>()};

            if (!ofs)
            {
                return false;
            }

            ofs << json.dump(1, '\t');
            return !ofs.bad();
        }

        bool load_artifact_meta(cstring_view source, artifact_meta& artifact)
        {
            std::ifstream ifs{source.as<std::string>()};

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
                const auto type = uuid::parse(it->get<std::string_view>());
                artifact.type = type ? *type : uuid{};
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

        bool ensure_directories(cstring_view directory)
        {
            if (!filesystem::create_directories(directory))
            {
                return false;
            }

            return filesystem::is_directory(directory).value_or(false);
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
        random_generator rng;
        uuid_random_generator uuidGenerator{rng};
        std::unordered_map<type_id, file_importer_info> importers;
        std::unordered_map<uuid, asset_entry> assets;
        string_builder assetsDir;
        string_builder artifactsDir;
        string_builder sourceFilesDir;

        string_builder& make_asset_path(string_builder& out, string_view directory)
        {
            if (filesystem::is_relative(directory))
            {
                out.append(assetsDir);
                out.append_path_separator();
            }

            out.append(directory);
            return out;
        }
    };

    asset_registry::asset_registry() = default;

    asset_registry::~asset_registry()
    {
        shutdown();
    }

    bool asset_registry::initialize(cstring_view assetsDir, cstring_view artifactsDir, cstring_view sourceFilesDir)
    {
        if (!ensure_directories(assetsDir) || !ensure_directories(artifactsDir) || !ensure_directories(sourceFilesDir))
        {
            return false;
        }

        m_impl = std::make_unique<impl>();

        m_impl->assetsDir.append(assetsDir).make_absolute_path();
        m_impl->artifactsDir.append(artifactsDir).make_absolute_path();
        m_impl->sourceFilesDir.append(sourceFilesDir).make_absolute_path();

        m_impl->rng.seed();

        return true;
    }

    void asset_registry::shutdown()
    {
        m_impl.reset();
    }

    bool asset_registry::create_directories(string_view directory)
    {
        string_builder sb;
        m_impl->make_asset_path(sb, directory);

        return ensure_directories(sb);
    }

    void asset_registry::register_file_importer(const file_importer_desc& desc)
    {
        const auto [it, inserted] = m_impl->importers.emplace(desc.type, file_importer_info{});

        if (inserted)
        {
            auto& info = it->second;
            info.create = desc.create;
            info.extensions.reserve(desc.extensions.size());

            for (const string_view ext : desc.extensions)
            {
                info.extensions.emplace_back(ext.as<string>());
            }
        }
    }

    void asset_registry::unregister_file_importer(type_id type)
    {
        m_impl->importers.erase(type);
    }

    importer asset_registry::create_importer(cstring_view sourceFile)
    {
        const auto ext = filesystem::extension(sourceFile);

        for (auto& [type, assetImporter] : m_impl->importers)
        {
            for (const auto& importerExt : assetImporter.extensions)
            {
                if (importerExt == ext)
                {
                    return importer{
                        generate_uuid(),
                        import_config{
                            .sourceFile = sourceFile.as<string>(),
                        },
                        type,
                        assetImporter.create(),
                    };
                }
            }
        }

        return {};
    }

    std::unique_ptr<file_importer> asset_registry::create_file_importer(cstring_view sourceFile) const
    {
        const auto ext = filesystem::extension(sourceFile);

        for (auto& [type, assetImporter] : m_impl->importers)
        {
            for (const auto& importerExt : assetImporter.extensions)
            {
                if (importerExt == ext)
                {
                    return assetImporter.create();
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
        const cstring_view srcArtifact,
        const artifact_meta& meta,
        write_policy policy)
    {
        char uuidBuffer[36];

        auto artifactPath = m_impl->artifactsDir;
        ensure_directories(artifactPath);

        artifactPath.append_path(artifactId.format_to(uuidBuffer));

        if (const auto exists = filesystem::exists(artifactPath).value_or(true);
            exists && policy == write_policy::no_overwrite)
        {
            return false;
        }
        else if (exists)
        {
            filesystem::remove(artifactPath).assert_value();
        }

        auto artifactMetaPath = artifactPath;
        artifactMetaPath.append(ArtifactMetaExtension);

        return filesystem::rename(srcArtifact, artifactPath).value_or(false) &&
            save_artifact_meta(meta, artifactMetaPath);
    }

    bool asset_registry::save_asset(string_view destination,
        string_view assetName,
        const asset_meta& meta,
        const deque<uuid>& artifacts,
        write_policy policy)
    {
        const auto [assetIt, insertedAsset] = m_impl->assets.emplace(meta.id,
            asset_entry{std::move(meta), std::vector<uuid>{artifacts.begin(), artifacts.end()}});

        if (!insertedAsset)
        {
            return false;
        }

        string_builder fullPath;
        m_impl->make_asset_path(fullPath, destination).append_path(assetName).append(AssetMetaExtension);

        if (policy == write_policy::no_overwrite && filesystem::exists(fullPath).value_or(true))
        {
            return false;
        }

        return save_asset_meta(assetIt->second.meta, artifacts, fullPath);
    }

    bool asset_registry::create_source_files_dir(string_builder& importDir, uuid importId)
    {
        char uuidBuffer[36];

        importDir.clear()
            .append(m_impl->sourceFilesDir)
            .append_path(importId.format_to(uuidBuffer).as<std::string_view>());

        if (!ensure_directories(importDir))
        {
            return false;
        }

        return true;
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

    bool asset_registry::find_asset_by_path(cstring_view path, uuid& id, asset_meta& assetMeta) const
    {
        string_builder fullPath;
        m_impl->make_asset_path(fullPath, path).append(AssetMetaExtension);

        return find_asset_by_meta_path(fullPath, id, assetMeta);
    }

    bool asset_registry::find_asset_by_meta_path(cstring_view path, uuid& id, asset_meta& assetMeta) const
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

        string_builder resourceFile;
        resourceFile.append(artifactsDir).append_path(id.format_to(uuidBuffer));

        if (!filesystem::exists(resourceFile).value_or(false))
        {
            return false;
        }

        auto resourceMeta = resourceFile;
        resourceMeta.append(ArtifactMetaExtension);

        return oblo::load_artifact_meta(resourceMeta, artifact);
    }

    void asset_registry::iterate_artifacts_by_type(const uuid& type,
        function_ref<bool(const uuid& assetId, const uuid& artifactId)> callback) const
    {
        artifact_meta meta{};

        for (const auto& [id, asset] : m_impl->assets)
        {
            for (const auto& artifactId : asset.artifacts)
            {
                if (load_artifact_meta(artifactId, meta) && meta.type == type)
                {
                    if (!callback(id, artifactId))
                    {
                        return;
                    }
                }
            }
        }
    }

    cstring_view asset_registry::get_asset_directory() const
    {
        return m_impl->assetsDir;
    }

    bool asset_registry::find_artifact_resource(
        const uuid& id, uuid& outType, string& outName, string& outPath, const void* userdata)
    {
        char uuidBuffer[36];

        auto* const self = static_cast<const asset_registry*>(userdata);
        const auto& artifactsDir = self->m_impl->artifactsDir;

        string_builder resourceFile;
        resourceFile.append(artifactsDir).append_path(id.format_to(uuidBuffer));

        if (!filesystem::exists(resourceFile).value_or(false))
        {
            return false;
        }

        auto resourceMeta = resourceFile;
        resourceMeta.append(ArtifactMetaExtension);

        artifact_meta meta;

        if (!oblo::load_artifact_meta(resourceMeta, meta))
        {
            return false;
        }

        outType = meta.type;
        outPath = resourceFile.as<string>();
        outName = std::move(meta.importName);

        return true;
    }

    void asset_registry::discover_assets()
    {
        std::error_code ec;

        for (auto&& entry :
            std::filesystem::recursive_directory_iterator{m_impl->assetsDir.view().as<std::string>(), ec})
        {
            const auto& p = entry.path();

            asset_meta meta{};
            std::vector<uuid> artifacts;

            if (load_asset_meta(meta, artifacts, p))
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