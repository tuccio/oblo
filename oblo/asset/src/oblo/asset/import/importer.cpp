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
#include <oblo/thread/parallel_for.hpp>

#include <nlohmann/json.hpp>

#include <fstream>

namespace oblo
{
    namespace
    {
        constexpr string_view ImportConfigFilename{"import.json"};

        bool write_import_config(const import_config& config, const type_id& importer, cstring_view destination)
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
        std::span<const import_node> nodes;
        std::span<const import_node_config> importNodesConfig;
        const data_document& settings;
        cstring_view temporaryPath;
        const importer::file_import_data* fileImportData;
        const deque<importer::file_import_data>* allImporters;
    };

    struct importer::file_import_data
    {
        std::unique_ptr<file_importer> importer;
        import_config config;
        import_preview preview;
        dynamic_array<import_node_config> nodeConfigs;
        usize firstChild;
        usize childrenCount;
        bool success;
    };

    importer::importer() = default;

    importer::importer(importer&&) noexcept = default;

    importer::importer(import_config config, const type_id& importerType, std::unique_ptr<file_importer> fileImporter) :
        m_importerType{importerType}
    {
        auto& root = m_fileImports.emplace_back();
        root.importer = std::move(fileImporter);
        root.config = std::move(config);
    }

    importer::~importer() = default;

    importer& importer::operator=(importer&&) noexcept = default;

    bool importer::init(const asset_registry& registry)
    {
        if (m_fileImports.size() != 1)
        {
            return false;
        }

        m_assetId = asset_registry::generate_uuid();

        for (usize i = 0; i < m_fileImports.size(); ++i)
        {
            auto& fi = m_fileImports[i];

            if (!fi.importer || !fi.importer->init(fi.config, fi.preview))
            {
                return false;
            }

            fi.nodeConfigs.assign(fi.preview.nodes.size(), {.enabled = true});

            fi.firstChild = m_fileImports.size();
            fi.childrenCount = fi.preview.children.size();

            if (!fi.preview.children.empty())
            {
                for (auto& child : fi.preview.children)
                {
                    auto& newChild = m_fileImports.emplace_back();
                    newChild.config = std::move(child);
                    newChild.importer = registry.create_file_importer(newChild.config.sourceFile);
                }
            }
        }

        return true;
    }

    bool importer::execute(const data_document& importSettings)
    {
        if (!begin_import())
        {
            return false;
        }

        string_builder temporaryPath;
        temporaryPath.format("./.asset_import/{}", m_assetId);

        filesystem::create_directories(temporaryPath).assert_value();

        parallel_for(
            [this, &importSettings, &temporaryPath](const job_range& r)
            {
                for (u32 i = r.begin; i < r.end; ++i)
                {
                    auto& fi = m_fileImports[i];

                    const import_context_impl contextImpl{
                        .nodes = fi.preview.nodes,
                        .importNodesConfig = fi.nodeConfigs,
                        .settings = i == 0 ? importSettings : fi.config.settings,
                        .temporaryPath = temporaryPath,
                        .fileImportData = &fi,
                        .allImporters = &m_fileImports,
                    };

                    import_context context;
                    context.m_impl = &contextImpl;

                    fi.success = fi.importer->import(context);
                }
            },
            job_range{0, m_fileImports.size32()},
            1u);

        for (const auto& fi : m_fileImports)
        {
            if (!fi.success)
            {
                return false;
            }
        }

        return true;
    }

    bool importer::finalize(asset_registry& registry, string_view destination)
    {
        if (!registry.create_directories(destination))
        {
            return false;
        }

        bool allSucceeded = true;

        deque<uuid> importedArtifacts;
        deque<cstring_view> sourceFiles;

        asset_meta assetMeta{
            .assetId = m_assetId,
            .sourceFileId = m_assetId,
            .isImported = true,
        };

        for (auto& fid : m_fileImports)
        {
            const auto results = fid.importer->get_results();

            sourceFiles.append(results.sourceFiles.begin(), results.sourceFiles.end());

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
                    log::error("Artifact '{}' ({}) will be skipped due to a UUID collision",
                        artifact.name,
                        artifact.id);
                    allSucceeded = false;
                    continue;
                }

                const artifact_meta meta{
                    .artifactId = artifact.id,
                    .type = artifact.type,
                    .sourceFileId = m_assetId,
                    .assetId = m_assetId,
                    .importName = artifact.name,
                };

                if (!registry.save_artifact(artifact.id, artifact.path, meta))
                {
                    log::error("Artifact '{}' ({}) will be skipped due to an error occurring while saving to disk",
                        artifact.name,
                        artifact.id);

                    allSucceeded = false;
                    continue;
                }

                importedArtifacts.emplace_back(artifact.id);

                if (&fid == &m_fileImports.front() && artifact.id == results.mainArtifactHint)
                {
                    assetMeta.mainArtifactHint = artifact.id;
                    assetMeta.typeHint = meta.type;
                }
            }
        }

        const auto assetFileName = filesystem::stem(m_fileImports.front().config.sourceFile);

        allSucceeded &= registry.save_asset(destination, assetFileName, std::move(assetMeta), importedArtifacts);
        allSucceeded &= write_source_files(registry, sourceFiles);

        // TODO: We might have to clean up on failure

        return allSucceeded;
    }

    bool importer::begin_import()
    {
        for (auto& imports : m_fileImports)
        {
            auto& importNodesConfig = imports.nodeConfigs;
            auto& nodes = imports.preview.nodes;

            if (importNodesConfig.size() != nodes.size())
            {
                return false;
            }

            const auto uuidGenerator = uuid_namespace_generator{m_assetId};

            for (usize i = 0; i < importNodesConfig.size(); ++i)
            {
                const auto& node = nodes[i];

                // TODO: Ensure that names are unique
                auto& config = importNodesConfig[i];
                const auto h = hash_all<hash>(node.name, node.artifactType, i);
                config.id = uuidGenerator.generate(std::as_bytes(std::span{&h, sizeof(h)}));

                const auto [artifactIt, artifactInserted] = m_artifacts.emplace(config.id,
                    artifact_meta{
                        .artifactId = config.id,
                        .type = node.artifactType,
                        .sourceFileId = m_assetId,
                        .assetId = m_assetId,
                        .importName = node.name,
                    });

                OBLO_ASSERT(artifactInserted);
            }
        }

        return true;
    }

    bool importer::is_valid() const noexcept
    {
        return !m_fileImports.empty();
    }

    const import_config& importer::get_config() const
    {
        return m_fileImports.front().config;
    }

    bool importer::write_source_files(asset_registry& registry, const deque<cstring_view>& sourceFiles)
    {
        string_builder importDir;

        if (!registry.create_source_files_dir(importDir, m_assetId))
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
        allSucceeded &= write_import_config(get_config(), m_importerType, pathBuilder);

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

    std::span<const import_node> import_context::get_child_import_nodes(usize i) const
    {
        OBLO_ASSERT(i < m_impl->fileImportData->childrenCount);
        const auto index = m_impl->fileImportData->firstChild + i;
        return m_impl->allImporters->at(index).preview.nodes;
    }

    std::span<const import_node_config> import_context::get_import_node_configs() const
    {
        return m_impl->importNodesConfig;
    }

    std::span<const import_node_config> import_context::get_child_import_node_configs(usize i) const
    {
        OBLO_ASSERT(i < m_impl->fileImportData->childrenCount);
        const auto index = m_impl->fileImportData->firstChild + i;
        return m_impl->allImporters->at(index).nodeConfigs;
    }

    const data_document& import_context::get_settings() const
    {
        return m_impl->settings;
    }
}