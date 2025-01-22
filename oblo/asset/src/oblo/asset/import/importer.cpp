#include <oblo/asset/import/importer.hpp>

#include <oblo/asset/asset_meta.hpp>
#include <oblo/asset/asset_registry.hpp>
#include <oblo/asset/import/import_artifact.hpp>
#include <oblo/asset/import/import_context.hpp>
#include <oblo/core/array_size.hpp>
#include <oblo/core/filesystem/filesystem.hpp>
#include <oblo/core/formatters/uuid_formatter.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/core/uuid_generator.hpp>
#include <oblo/log/log.hpp>

#include <nlohmann/json.hpp>

#include <fstream>

namespace oblo
{
    namespace
    {
        constexpr string_view ImportConfigFilename{"import.json"};

        bool write_import_config(const importer_config& config, const type_id& importer, cstring_view destination)
        {
            nlohmann::ordered_json json;

            if (!importer.name.empty())
            {
                json["importer"] = importer.name;
            }

            json["filename"] = filesystem::filename(config.sourceFile).as<std::string_view>();
            json["source"] = config.sourceFile.as<std::string_view>();

            std::ofstream ofs{destination.as<std::string>()};

            if (!ofs)
            {
                return false;
            }

            ofs << json.dump(1, '\t');
            return !ofs.bad();
        }
    }

    struct import_context_impl
    {
        asset_registry& registry;
        std::span<const import_node> nodes;
        std::span<const import_node_config> importNodesConfig;
        uuid importUuid;
        const data_document& settings;
        string_builder temporaryPath;
    };

    importer::importer() = default;

    importer::importer(importer&&) noexcept = default;

    importer::importer(
        importer_config config, const type_id& importerType, std::unique_ptr<file_importer> fileImporter) :
        m_config{std::move(config)}, m_importer{std::move(fileImporter)}, m_importerType{importerType}
    {
    }

    importer::~importer() = default;

    importer& importer::operator=(importer&&) noexcept = default;

    bool importer::init()
    {
        if (!m_importer)
        {
            return false;
        }

        if (!m_importer->init(m_config, m_preview))
        {
            return false;
        }

        m_importNodesConfig.assign(m_preview.nodes.size(), {.enabled = true});
        return true;
    }

    bool importer::execute(string_view destinationDir, const data_document& importSettings)
    {
        if (!begin_import(*m_config.registry, m_importNodesConfig))
        {
            return false;
        }

        const auto importUuid = m_config.registry->generate_uuid();

        import_context_impl contextImpl{
            .registry = *m_config.registry,
            .nodes = m_preview.nodes,
            .importNodesConfig = m_importNodesConfig,
            .importUuid = importUuid,
            .settings = importSettings,
        };

        contextImpl.temporaryPath.format("./.asset_import/{}", importUuid);

        filesystem::create_directories(contextImpl.temporaryPath).assert_value();

        import_context context;
        context.m_impl = &contextImpl;

        if (!m_importer->import(context))
        {
            // TODO: Cleanup
            return false;
        }

        // TODO: Cleanup if finalize_import fails too, e.g. remove the saved artifacts, if any
        return finalize_import(*m_config.registry, destinationDir);
    }

    bool importer::begin_import(asset_registry& registry, std::span<import_node_config> importNodesConfig)
    {
        // TODO: Could maybe create the folder here to ensure it's unique
        m_importId = registry.generate_uuid();

        if (importNodesConfig.size() != m_preview.nodes.size())
        {
            return false;
        }

        const auto uuidGenerator = uuid_namespace_generator{m_importId};

        for (usize i = 0; i < importNodesConfig.size(); ++i)
        {
            const auto& node = m_preview.nodes[i];

            if (!registry.has_asset_type(node.type))
            {
                return false;
            }

            // TODO: Ensure that names are unique
            auto& config = importNodesConfig[i];
            const auto h = hash_all<hash>(node.name, node.type, i);
            config.id = uuidGenerator.generate(std::as_bytes(std::span{&h, sizeof(h)}));

            const auto [artifactIt, artifactInserted] = m_artifacts.emplace(config.id,
                artifact_meta{
                    .id = config.id,
                    .type = node.type,
                    .importId = m_importId,
                    .importName = node.name,
                });

            OBLO_ASSERT(artifactInserted);
        }

        return true;
    }

    bool importer::finalize_import(asset_registry& registry, string_view destination)
    {
        if (!registry.create_directories(destination))
        {
            return false;
        }

        bool allSucceeded = true;

        const auto results = m_importer->get_results();

        asset_meta assetMeta{
            .id = m_importId,
            .isImported = true,
        };

        std::vector<uuid> importedArtifacts;
        importedArtifacts.reserve(results.artifacts.size());

        for (const import_artifact& artifact : results.artifacts)
        {
            if (artifact.id.is_nil())
            {
                log::error("Artifact '{}' will be skipped due to invalid UUID (this may signal a bug in "
                           "the importer)",
                    artifact.name);
                allSucceeded = false;
                continue;
            }

            if (artifact.path.empty())
            {
                log::error("Artifact '{}' will be skipped because no output was produced (this may signal a bug in "
                           "the importer)",
                    artifact.name);
                allSucceeded = false;
                continue;
            }

            const auto artifactIt = m_artifacts.find(artifact.id);

            if (artifactIt == m_artifacts.end())
            {
                log::error("Artifact '{}' ({}) will be skipped due to a UUID collision", artifact.name, artifact.id);
                allSucceeded = false;
                continue;
            }

            const artifact_meta meta{
                .id = artifact.id,
                .type = artifact.type,
                .importId = m_importId,
                .importName = artifact.name,
            };

            if (!registry.save_artifact(artifact.id, artifact.type, artifact.path, meta))
            {
                log::error("Artifact '{}' ({}) will be skipped due to an error occurring while saving to disk",
                    artifact.name,
                    artifact.id);

                allSucceeded = false;
                continue;
            }

            importedArtifacts.emplace_back(artifact.id);

            if (artifact.id == results.mainArtifactHint)
            {
                assetMeta.mainArtifactHint = artifact.id;
                assetMeta.typeHint = meta.type;
            }
        }

        const auto assetFileName = filesystem::stem(m_config.sourceFile);

        allSucceeded &= registry.save_asset(destination, assetFileName, std::move(assetMeta), importedArtifacts);
        allSucceeded &= write_source_files(results.sourceFiles);

        return allSucceeded;
    }

    bool importer::is_valid() const noexcept
    {
        return m_importer != nullptr;
    }

    const importer_config& importer::get_config() const
    {
        return m_config;
    }

    uuid importer::get_import_id() const
    {
        return m_importId;
    }

    bool importer::write_source_files(std::span<const string> sourceFiles)
    {
        string_builder importDir;

        if (!m_config.registry->create_source_files_dir(importDir, m_importId))
        {
            return false;
        }

        bool allSucceeded = true;

        string_builder pathBuilder = importDir;

        for (const auto& sourceFile : sourceFiles)
        {
            pathBuilder.clear().append(importDir).append(filesystem::filename(sourceFile));
            allSucceeded &= filesystem::copy_file(sourceFile, pathBuilder).value_or(false);
        }

        pathBuilder.clear().append(importDir).append(ImportConfigFilename);
        allSucceeded &= write_import_config(m_config, m_importerType, pathBuilder);

        return allSucceeded;
    }

    cstring_view import_context::get_output_path(const uuid& id, string_builder& outPath) const
    {
        outPath = m_impl->temporaryPath;
        outPath.append_path_separator().format("{}", id);
        return outPath;
    }

    std::span<const import_node> import_context::get_import_nodes() const
    {
        return m_impl->nodes;
    }

    std::span<const import_node_config> import_context::get_import_node_configs() const
    {
        return m_impl->importNodesConfig;
    }

    uuid import_context::get_import_uuid() const
    {
        return m_impl->importUuid;
    }

    const data_document& import_context::get_settings() const
    {
        return m_impl->settings;
    }
}