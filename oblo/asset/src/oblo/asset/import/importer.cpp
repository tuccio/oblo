#include <oblo/asset/import/importer.hpp>

#include <oblo/asset/asset_meta.hpp>
#include <oblo/asset/asset_registry_impl.hpp>
#include <oblo/asset/import/file_importer.hpp>
#include <oblo/asset/import/import_artifact.hpp>
#include <oblo/asset/import/import_context.hpp>
#include <oblo/core/array_size.hpp>
#include <oblo/core/filesystem/filesystem.hpp>
#include <oblo/core/formatters/uuid_formatter.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/core/uuid_generator.hpp>
#include <oblo/log/log.hpp>
#include <oblo/properties/property_value_wrapper.hpp>
#include <oblo/properties/serialization/data_document.hpp>
#include <oblo/properties/serialization/json.hpp>
#include <oblo/thread/parallel_for.hpp>
#include <oblo/trace/profile.hpp>

namespace oblo
{
    namespace
    {
        constexpr string_view g_importConfigName{"config.oimport"};

        bool write_import_config(const import_config& config, cstring_view destination, bool omitSourceFilesPath)
        {
            data_document doc;

            doc.init();

            if (!omitSourceFilesPath)
            {
                doc.child_value(doc.get_root(), "source"_hsv, property_value_wrapper{config.sourceFile});
            }

            const auto filename = property_value_wrapper{filesystem::filename(cstring_view{config.sourceFile})};
            doc.child_value(doc.get_root(), "filename"_hsv, filename);

            return json::write(doc, destination).has_value();
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
        unique_ptr<file_importer> importer;
        import_config config;
        import_preview preview;
        dynamic_array<import_node_config> nodeConfigs;
        usize firstChild;
        usize childrenCount;
        bool success;
    };

    bool importer::read_source_file_path(
        const asset_registry_impl& registry, uuid assetId, h32<asset_repository> assetSource, string_builder& out)
    {
        registry.make_source_files_dir_path(out, assetId, assetSource).append_path(g_importConfigName);

        data_document doc;

        if (!json::read(doc, out))
        {
            return false;
        }

        const auto c = doc.find_child(doc.get_root(), "filename"_hsv);

        if (c == data_node::Invalid)
        {
            return false;
        }

        if (auto r = doc.read_string(c))
        {
            registry.make_source_files_dir_path(out, assetId, assetSource).append_path(r->str());
            return true;
        }

        return false;
    }

    importer::importer() = default;

    importer::importer(importer&&) noexcept = default;

    importer::importer(import_config config, unique_ptr<file_importer> fileImporter)
    {
        auto& root = m_fileImports.emplace_back();
        root.importer = std::move(fileImporter);
        root.config = std::move(config);
    }

    importer::~importer() = default;

    importer& importer::operator=(importer&&) noexcept = default;

    bool importer::init(const asset_registry_impl& registry, uuid assetId, cstring_view workDir, bool isReimport)
    {
        OBLO_PROFILE_SCOPE();
        OBLO_PROFILE_TAG(get_config().sourceFile);

        if (m_fileImports.size() != 1)
        {
            return false;
        }

        m_assetId = assetId;
        m_isReimport = isReimport;
        m_temporaryPath = workDir;

        for (usize i = 0; i < m_fileImports.size(); ++i)
        {
            auto& fi = m_fileImports[i];
            fi.config.workDir = m_temporaryPath;

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

    void importer::set_native_asset_type(uuid nativeAssetType)
    {
        m_nativeAssetType = nativeAssetType;
    }

    void importer::set_asset_name(string_view assetName)
    {
        m_assetName = assetName;
    }

    bool importer::execute(const data_document& importSettings)
    {
        OBLO_PROFILE_SCOPE();
        OBLO_PROFILE_TAG(get_config().sourceFile);

        if (!begin_import())
        {
            return false;
        }

        parallel_for(
            [this, &importSettings](const job_range& r)
            {
                for (u32 i = r.begin; i < r.end; ++i)
                {
                    auto& fi = m_fileImports[i];

                    const import_context_impl contextImpl{
                        .nodes = fi.preview.nodes,
                        .importNodesConfig = fi.nodeConfigs,
                        .settings = i == 0 ? importSettings : fi.config.settings,
                        .temporaryPath = m_temporaryPath,
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

    bool importer::finalize(asset_registry_impl& registry, cstring_view destination, h32<asset_repository> assetSource)
    {
        using write_policy = asset_registry_impl::write_policy;

        const write_policy writePolicy = m_isReimport ? write_policy::overwrite : write_policy::no_overwrite;

        string_builder destinationDir;

        if (m_isReimport)
        {
            OBLO_ASSERT(destination.empty());

            auto* const assetFSPath = registry.get_asset_filesystem_path(m_assetId);

            if (!assetFSPath)
            {
                return false;
            }

            destinationDir.append(*assetFSPath).parent_path();
            destination = destinationDir;
        }

        if (!registry.create_directories(destination))
        {
            return false;
        }

        bool allSucceeded = true;

        deque<uuid> importedArtifacts;
        deque<cstring_view> sourceFiles;

        asset_meta assetMeta{
            .assetId = m_assetId,
            .nativeAssetType = m_nativeAssetType,
        };

        const uuid processId = asset_registry_impl::generate_uuid();

        string_builder artifactsPath;
        registry.make_artifacts_directory_path(artifactsPath, m_assetId, processId);

        filesystem::remove_all(artifactsPath).assert_value();

        if (!filesystem::create_directories(artifactsPath).value_or(false))
        {
            log::error("Failed to create artifacts directory {}", artifactsPath);
            return false;
        }

        for (auto& fid : m_fileImports)
        {
            const auto results = fid.importer->get_results();

            if (!fid.config.skipSourceFiles)
            {
                sourceFiles.append(results.sourceFiles.begin(), results.sourceFiles.end());
            }

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
                    .assetId = m_assetId,
                    .name = artifact.name,
                };

                if (!registry.save_artifact(artifact.path, meta, processId, writePolicy))
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

        const auto assetFileName =
            m_assetName.empty() ? filesystem::stem(m_fileImports.front().config.sourceFile) : m_assetName;

        allSucceeded &= registry.save_asset(destination,
            assetFileName,
            std::move(assetMeta),
            processId,
            importedArtifacts,
            writePolicy);

        allSucceeded &= write_source_files(registry, sourceFiles, assetSource);

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
                config.id = uuidGenerator.generate(std::as_bytes(std::span{&h, 1}));

                const auto [artifactIt, artifactInserted] = m_artifacts.emplace(config.id,
                    artifact_meta{
                        .artifactId = config.id,
                        .type = node.artifactType,
                        .assetId = m_assetId,
                        .name = node.name,
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

    bool importer::is_reimport() const noexcept
    {
        return m_isReimport;
    }

    const import_config& importer::get_config() const
    {
        return m_fileImports.front().config;
    }

    uuid importer::get_asset_id() const
    {
        return m_assetId;
    }

    bool importer::write_source_files(
        asset_registry_impl& registry, const deque<cstring_view>& sourceFiles, h32<asset_repository> repoId)
    {
        string_builder importDir;

        if (!registry.create_source_files_dir(importDir, m_assetId, repoId))
        {
            return false;
        }

        bool allSucceeded = true;

        string_builder pathBuilder = importDir;

        for (const auto& sourceFile : sourceFiles)
        {
            if (sourceFile.starts_with(importDir.view()))
            {
                continue;
            }

            pathBuilder.clear().append(importDir).append_path(filesystem::filename(sourceFile));
            allSucceeded &= filesystem::copy_file(sourceFile, pathBuilder).value_or(false);
        }

        pathBuilder.clear().append(importDir).append_path(g_importConfigName);

        auto& repository = registry.get_asset_repository(repoId);

        const bool omitImportSource = repository.flags.contains(asset_repository_flags::omit_import_source_path);
        allSucceeded &= write_import_config(get_config(), pathBuilder, omitImportSource);

        return allSucceeded;
    }

    cstring_view import_context::get_output_path(
        const uuid& id, string_builder& outPath, string_view optExtension) const
    {
        OBLO_ASSERT(optExtension.empty() || optExtension.starts_with("."));
        outPath = m_impl->temporaryPath;
        outPath.append_path_separator().format("{}", id).append(optExtension);
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