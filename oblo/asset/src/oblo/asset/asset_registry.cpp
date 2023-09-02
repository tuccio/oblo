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

        struct pending_asset_import
        {
            std::vector<import_artifact> artifacts;
        };

        struct pending_import
        {
            std::vector<pending_asset_import> assets;
            std::unordered_map<uuid, artifact_meta> artifacts;
        };

        void save_asset_meta(const asset_meta& meta, const std::filesystem::path& destination)
        {
            nlohmann::json json;
            json["type"] = meta.type.name;

            auto artifacts = nlohmann::json::array();

            char uuidBuffer[36];

            for (auto& artifact : meta.artifacts)
            {
                nlohmann::json jsonArtifact;

                artifact.id.format_to(uuidBuffer);

                jsonArtifact["id"] = std::string_view{uuidBuffer, array_size(uuidBuffer)};
                jsonArtifact["name"] = artifact.name;

                artifacts.emplace_back(std::move(jsonArtifact));
            }

            json["artifacts"] = std::move(artifacts);

            std::ofstream ofs{destination};
            ofs << json.dump(4);
        }

        bool ensure_directories(const std::filesystem::path& directory)
        {
            std::error_code ec;
            std::filesystem::create_directories(directory, ec);
            return std::filesystem::is_directory(directory, ec);
        }

        constexpr std::string_view assetExt{".oasset"};
    }

    struct asset_registry::impl
    {
        uuid_random_generator uuidGenerator;
        std::unordered_map<type_id, asset_type_info> assetTypes;
        std::unordered_map<type_id, file_importer_info> importers;
        std::unordered_map<uuid, asset_meta> assets;
        std::filesystem::path assetsDir;
        std::filesystem::path artifactsDir;
        std::unordered_map<uuid, pending_import> pendingImports;
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
                            .assetManager = this,
                            .sourceFile = sourceFile,
                        },
                        assetImporter.create(),
                    };
                }
            }
        }

        return {};
    }

    bool asset_registry::add_imported_asset(uuid importUuid,
                                            import_artifact mainArtifact,
                                            std::span<import_artifact> otherArtifacts)
    {
        const auto importIt = m_impl->pendingImports.find(importUuid);

        if (importIt == m_impl->pendingImports.end())
        {
            return false;
        }

        auto& pendingImport = importIt->second;

        const auto it = pendingImport.artifacts.find(mainArtifact.id);

        if (it == pendingImport.artifacts.end())
        {
            return false;
        }

        if (mainArtifact.data.empty())
        {
            return false;
        }

        if (mainArtifact.data.get_type() != it->second.type)
        {
            return false;
        }

        it->second.id = mainArtifact.id;

        auto& importedAsset = pendingImport.assets.emplace_back();

        importedAsset.artifacts.clear();
        importedAsset.artifacts.reserve(otherArtifacts.size() + 1);
        importedAsset.artifacts.emplace_back(std::move(mainArtifact));

        for (auto& otherArtifact : otherArtifacts)
        {
            const auto currentArtifactIt = pendingImport.artifacts.find(otherArtifact.id);

            if (currentArtifactIt == pendingImport.artifacts.end())
            {
                OBLO_ASSERT(false);
                continue;
            }

            currentArtifactIt->second.id = otherArtifact.id;
            importedAsset.artifacts.emplace_back(std::move(otherArtifact));
        }

        return true;
    }

    uuid asset_registry::begin_import(const import_preview& preview, std::span<import_node_config> importNodesConfig)
    {
        if (importNodesConfig.size() != preview.nodes.size())
        {
            return {};
        }

        const uuid importUuid = m_impl->uuidGenerator.generate();
        const auto [importIt, inserted] = m_impl->pendingImports.emplace(importUuid, pending_import{});
        OBLO_ASSERT(inserted);

        for (usize i = 0; i < importNodesConfig.size(); ++i)
        {
            const auto& node = preview.nodes[i];

            if (m_impl->assetTypes.find(node.type) == m_impl->assetTypes.end())
            {
                return {};
            }

            auto& config = importNodesConfig[i];
            config.id = m_impl->uuidGenerator.generate();

            const auto [artifactIt, artifactInserted] = importIt->second.artifacts.emplace(config.id,
                                                                                           artifact_meta{
                                                                                               .type = node.type,
                                                                                               .name = node.name,
                                                                                           });

            OBLO_ASSERT(artifactInserted);
        }

        return importUuid;
    }

    bool asset_registry::finalize_import(const uuid& importUuid, const std::filesystem::path& destination)
    {
        const auto importIt = m_impl->pendingImports.find(importUuid);

        if (importIt == m_impl->pendingImports.end())
        {
            return false;
        }

        auto poppedImport = m_impl->pendingImports.extract(importIt);
        const pending_import& import = poppedImport.mapped();

        char uuidBuffer[36];

        std::string assetFileName;

        const auto fullDestinationPath = m_impl->assetsDir / destination;
        ensure_directories(fullDestinationPath);

        for (const pending_asset_import& assetImport : import.assets)
        {
            OBLO_ASSERT(!assetImport.artifacts.empty(), "The first artifact is expected to be the asset");

            const auto& mainArtifact = assetImport.artifacts[0];
            const auto [assetIt, insertedAsset] = m_impl->assets.emplace(mainArtifact.id,
                                                                         asset_meta{
                                                                             .type = mainArtifact.data.get_type(),
                                                                         });

            const auto mainArtifactIt = import.artifacts.find(mainArtifact.id);

            if (mainArtifactIt == import.artifacts.end())
            {
                OBLO_ASSERT(false);
                continue;
            }

            assetFileName = mainArtifactIt->second.name;
            assetFileName.append(assetExt);

            if (!insertedAsset)
            {
                continue;
            }

            for (const import_artifact& artifact : assetImport.artifacts)
            {
                const auto artifactIt = import.artifacts.find(artifact.id);

                if (artifactIt == import.artifacts.end())
                {
                    OBLO_ASSERT(false);
                    continue;
                }

                auto* const artifactPtr = artifact.data.try_get();

                if (!artifactPtr)
                {
                    OBLO_ASSERT(false); // TODO: Log?
                    continue;
                }

                artifact.id.format_to(uuidBuffer);

                const auto assetType = artifact.data.get_type();
                const auto saveFunction = m_impl->assetTypes.at(assetType).save;
                saveFunction(artifactPtr, m_impl->artifactsDir / std::string_view{uuidBuffer, array_size(uuidBuffer)});

                assetIt->second.artifacts.emplace_back(std::move(artifactIt->second));
            }

            // TODO: Copy source files

            save_asset_meta(assetIt->second, fullDestinationPath / assetFileName);
        }

        return true;
    }
}