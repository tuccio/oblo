#include <oblo/asset/asset_registry.hpp>

#include <oblo/asset/any_asset.hpp>
#include <oblo/asset/asset_meta.hpp>
#include <oblo/asset/asset_path.hpp>
#include <oblo/asset/asset_registry_impl.hpp>
#include <oblo/asset/descriptors/file_importer_descriptor.hpp>
#include <oblo/asset/descriptors/native_asset_descriptor.hpp>
#include <oblo/asset/import/file_importer.hpp>
#include <oblo/asset/import/import_artifact.hpp>
#include <oblo/asset/import/import_preview.hpp>
#include <oblo/asset/import/importer.hpp>
#include <oblo/core/array_size.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/filesystem/directory_watcher.hpp>
#include <oblo/core/filesystem/filesystem.hpp>
#include <oblo/core/formatters/uuid_formatter.hpp>
#include <oblo/core/invoke/function_ref.hpp>
#include <oblo/core/platform/core.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/core/uuid_generator.hpp>
#include <oblo/log/log.hpp>
#include <oblo/properties/serialization/common.hpp>
#include <oblo/resource/providers/resource_provider.hpp>
#include <oblo/thread/job_manager.hpp>

#include <atomic>
#include <filesystem>
#include <unordered_map>

namespace oblo
{
    struct asset_entry
    {
        asset_meta meta;
        dynamic_array<uuid> artifacts;

        /// @brief The asset path is the full path without the file extension
        string_builder path;

        bool isProcessing{};

        h32<asset_repository> assetSource{};
    };

    struct file_importer_info
    {
        create_file_importer_fn create;
        dynamic_array<string> extensions;
    };

    struct asset_process_info
    {
        // Could maybe include a hash of settings or timestamps to determine when to re-run an importer or a
        // processor
        deque<uuid> artifacts;
        uuid processId;

        void clear()
        {
            artifacts.clear();
        }
    };

    struct import_process
    {
        importer importer;
        data_document settings;
        string destination;
        time startTime;
        h32<asset_repository> assetSource{};
        bool isReimport;
        job_handle job{};
        std::atomic<bool> success{};
    };

    struct artifact_entry
    {
        artifact_meta meta;
        uuid processId;
    };

    namespace
    {
        constexpr std::string_view g_artifactMetaExtension{".oartifact"};
        constexpr string_view g_assetProcessExtension{".oproc"};

        bool load_asset_meta(asset_meta& meta, cstring_view path)
        {
            data_document doc;

            if (!json::read(doc, path))
            {
                return false;
            }

            const auto root = doc.get_root();

            meta.assetId = doc.read_uuid(doc.find_child(root, "assetId"_hsv)).value_or(uuid{});
            meta.mainArtifactHint = doc.read_uuid(doc.find_child(root, "mainArtifactHint"_hsv)).value_or(uuid{});
            meta.typeHint = doc.read_uuid(doc.find_child(root, "typeHint"_hsv)).value_or(uuid{});
            meta.nativeAssetType = doc.read_uuid(doc.find_child(root, "nativeAssetType"_hsv)).value_or(uuid{});

            return !meta.assetId.is_nil();
        }

        bool load_asset_meta(asset_meta& meta, const std::filesystem::path& path)
        {
            return load_asset_meta(meta, cstring_view{path.string().c_str()});
        }

        bool save_asset_meta(const asset_meta& meta, cstring_view destination)
        {
            data_document doc;
            doc.init();

            const auto root = doc.get_root();

            doc.child_value(root, "assetId"_hsv, property_value_wrapper{meta.assetId});
            doc.child_value(root, "mainArtifactHint"_hsv, property_value_wrapper{meta.mainArtifactHint});
            doc.child_value(root, "typeHint"_hsv, property_value_wrapper{meta.typeHint});

            if (!meta.nativeAssetType.is_nil())
            {
                doc.child_value(root, "nativeAssetType"_hsv, property_value_wrapper{meta.nativeAssetType});
            }

            return json::write(doc, destination).has_value();
        }

        // TODO: Need to read the full meta instead
        bool load_asset_id_from_meta(cstring_view path, uuid& id)
        {
            data_document doc;

            if (!json::read(doc, path))
            {
                return false;
            }

            const auto root = doc.get_root();

            const auto assetId = doc.read_uuid(doc.find_child(root, "assetId"_hsv));

            if (assetId)
            {
                id = *assetId;
            }

            return assetId.has_value();
        }

        bool save_artifact_meta(const artifact_meta& meta, cstring_view destination)
        {
            data_document doc;
            doc.init();

            const auto root = doc.get_root();

            doc.child_value(root, "artifactId"_hsv, property_value_wrapper{meta.artifactId});
            doc.child_value(root, "type"_hsv, property_value_wrapper{meta.type});
            doc.child_value(root, "assetId"_hsv, property_value_wrapper{meta.assetId});
            doc.child_value(root, "name"_hsv, property_value_wrapper{meta.name});

            return json::write(doc, destination).has_value();
        }

        bool load_artifact_meta(cstring_view source, artifact_meta& meta)
        {
            data_document doc;

            if (!json::read(doc, source))
            {
                return false;
            }

            const auto root = doc.get_root();

            meta.artifactId = doc.read_uuid(doc.find_child(root, "artifactId"_hsv)).value_or(uuid{});
            meta.type = doc.read_uuid(doc.find_child(root, "type"_hsv)).value_or(uuid{});
            meta.assetId = doc.read_uuid(doc.find_child(root, "assetId"_hsv)).value_or(uuid{});
            meta.name = doc.read_string(doc.find_child(root, "name"_hsv)).value_or({}).str();

            return true;
        }

        bool load_asset_process_info(cstring_view source, asset_process_info& info)
        {
            data_document doc;

            if (!json::read(doc, source))
            {
                return false;
            }

            const auto processId = doc.find_child(doc.get_root(), "processId"_hsv);

            if (processId == data_node::Invalid)
            {
                return false;
            }

            if (const auto r = doc.read_uuid(processId); !r)
            {
                return false;
            }
            else
            {
                info.processId = *r;
            }

            const auto artifacts = doc.find_child(doc.get_root(), "artifacts"_hsv);

            if (artifacts == data_node::Invalid || !doc.is_array(artifacts))
            {
                return false;
            }

            for (const u32 child : doc.children(artifacts))
            {
                const auto uuid = doc.read_uuid(child);

                if (!uuid)
                {
                    return false;
                }

                info.artifacts.emplace_back(*uuid);
            }

            return true;
        }

        bool save_asset_process_info(const asset_process_info& info, cstring_view destination)
        {
            data_document doc;
            doc.init();

            doc.child_value(doc.get_root(), "processId"_hsv, property_value_wrapper{info.processId});

            const auto artifacts = doc.child_array(doc.get_root(), "artifacts"_hsv);

            for (const uuid& id : info.artifacts)
            {
                const auto e = doc.array_push_back(artifacts);
                doc.make_value(e, property_value_wrapper{id});
            }

            return json::write(doc, destination).has_value();
        }

        bool ensure_directories(cstring_view directory)
        {
            if (!filesystem::create_directories(directory))
            {
                return false;
            }

            return filesystem::is_directory(directory).value_or(false);
        }

        bool is_asset_path(string_view assetPath)
        {
            return assetPath.starts_with(asset_path_prefix);
        }
    }

    class artifact_resource_provider final : public resource_provider
    {
    public:
        explicit artifact_resource_provider(const asset_registry_impl& impl) : m_impl{impl} {}

        void iterate_resource_events(on_add_fn onAdd, on_remove_fn onRemove, on_update_fn onUpdate)
        {
            string_builder path;

            for (const auto& e : m_events)
            {
                switch (e.kind)
                {
                case event_kind::add: {
                    auto it = m_impl.artifactsMap.find(e.artifactId);

                    if (it == m_impl.artifactsMap.end())
                    {
                        continue;
                    }

                    m_impl.make_artifact_path(path, e.assetId, it->second.processId, e.artifactId);

                    const resource_added_event resourceEvent{
                        .id = e.artifactId,
                        .typeUuid = it->second.meta.type,
                        .name = it->second.meta.name,
                        .path = path,
                    };

                    onAdd(resourceEvent);
                }

                break;

                case event_kind::remove: {
                    const resource_removed_event resourceEvent{
                        .id = e.artifactId,
                    };

                    onRemove(resourceEvent);
                }

                break;

                case event_kind::update: {
                    auto it = m_impl.artifactsMap.find(e.artifactId);

                    if (it == m_impl.artifactsMap.end())
                    {
                        continue;
                    }

                    m_impl.make_artifact_path(path, e.assetId, it->second.processId, e.artifactId);

                    const resource_updated_event resourceEvent{
                        .id = e.artifactId,
                        .typeUuid = it->second.meta.type,
                        .name = it->second.meta.name,
                        .path = path,
                    };

                    onUpdate(resourceEvent);
                }

                break;

                default:
                    unreachable();
                }
            }

            m_events.clear();
        }

        void push_removed_artifact(uuid artifactId)
        {
            m_events.push_back_default() = {
                .kind = event_kind::remove,
                .artifactId = artifactId,
            };
        }

        void push_added_artifact(uuid assetId, uuid artifactId)
        {
            m_events.push_back_default() = {
                .kind = event_kind::add,
                .assetId = assetId,
                .artifactId = artifactId,
            };
        }

        void push_modified_artifact(uuid assetId, uuid artifactId)
        {
            m_events.push_back_default() = {
                .kind = event_kind::update,
                .assetId = assetId,
                .artifactId = artifactId,
            };
        }

    private:
        enum class event_kind : u8
        {
            add,
            remove,
            update,
        };

        struct artifact_event
        {
            event_kind kind;
            uuid assetId;
            uuid artifactId;
        };

    private:
        const asset_registry_impl& m_impl{};
        deque<artifact_event> m_events;
    };

    asset_registry::asset_registry() = default;

    asset_registry::~asset_registry()
    {
        shutdown();
    }

    bool asset_registry::initialize(std::span<const asset_repository_descriptor> assetRepositories,
        cstring_view artifactsDir)
    {
        if (!ensure_directories(artifactsDir))
        {
            return false;
        }

        m_impl = allocate_unique<asset_registry_impl>();

        m_impl->artifactsDir.append(artifactsDir).make_absolute_path();
        m_impl->assetRepositories.resize(assetRepositories.size() + 1);

        h32<asset_repository> lastIdx{};

        for (const auto& repoDesc : assetRepositories)
        {
            if (!ensure_directories(repoDesc.assetsDirectory) || !ensure_directories(repoDesc.sourcesDirectory))
            {
                return false;
            }

            ++lastIdx.value;

            auto& source = m_impl->assetRepositories[lastIdx.value];

            // NOTE: We store the string and keep the view in the map, so the array should not be resized anymore
            source.name = repoDesc.name.as<string>();
            hashed_string_view sourceName{source.name};

            const auto [mapIt, ok] = m_impl->assetSourceNameToIdx.emplace(sourceName, lastIdx);

            if (!ok)
            {
                return false;
            }

            source.assetDir.append(repoDesc.assetsDirectory).make_absolute_path();
            source.sourceDir.append(repoDesc.sourcesDirectory).make_absolute_path();
            source.flags = repoDesc.flags;
            source.id = lastIdx;
            source.watcher = allocate_unique<filesystem::directory_watcher>();

            if (!source.watcher->init({.path = source.assetDir.view(), .isRecursive = true}))
            {
                return false;
            }
        }

        return true;
    }

    void asset_registry::shutdown()
    {
        if (m_impl)
        {
            while (get_running_import_count() > 0)
            {
                update();
            }

            m_impl.reset();
        }
    }

    expected<uuid> asset_registry::import(
        string_view sourceFile, string_view destination, string_view assetName, data_document settings)
    {
        OBLO_ASSERT(is_asset_path(destination));

        string_builder fullPath;

        const auto assetSource = m_impl->resolve_asset_path(fullPath, destination);

        if (!assetSource)
        {
            return "Failed to register asset"_err;
        }

        auto importer = m_impl->create_importer(sourceFile);

        if (!importer.is_valid())
        {
            return "Failed to register asset"_err;
        }

        const uuid assetId = asset_registry_impl::generate_uuid();

        string_builder workDir;

        if (!m_impl->create_temporary_files_dir(workDir, assetId))
        {
            return "Failed to register asset"_err;
        }

        if (!importer.init(*m_impl, assetId, workDir, false))
        {
            return "Asset operation failed"_err;
        }

        if (!assetName.empty())
        {
            importer.set_asset_name(assetName);
        }

        m_impl->push_import_process(nullptr,
            std::move(importer),
            std::move(settings),
            cstring_view{fullPath},
            assetSource);

        return assetId;
    }

    void asset_registry::register_file_importer(const file_importer_descriptor& desc)
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

    void asset_registry::register_native_asset_type(native_asset_descriptor desc)
    {
        m_impl->nativeAssetTypes.emplace(desc.typeUuid, std::move(desc));
    }

    void asset_registry::unregister_native_asset_type(uuid type)
    {
        m_impl->nativeAssetTypes.erase(type);
    }

    expected<> asset_registry::process(uuid asset, data_document* optSettings)
    {
        const auto it = m_impl->assets.find(asset);

        if (it == m_impl->assets.end())
        {
            return "Asset not found"_err;
        }

        if (it->second.isProcessing)
        {
            return "Asset operation failed"_err;
        }

        // TODO: Maybe check if it's being reprocessed
        const auto& meta = it->second.meta;

        // This means it requires a reimport
        string_builder fileSourcePath;

        if (!importer::read_source_file_path(*m_impl, meta.assetId, it->second.assetSource, fileSourcePath))
        {
            return "Asset not found"_err;
        }

        oblo::importer importer;

        if (const auto nativeAssetIt = m_impl->nativeAssetTypes.find(meta.nativeAssetType);
            nativeAssetIt != m_impl->nativeAssetTypes.end())
        {
            importer = oblo::importer{
                import_config{
                    .sourceFile = fileSourcePath.as<string>(),
                },
                nativeAssetIt->second.createImporter(nativeAssetIt->second.userdata),
            };

            importer.set_native_asset_type(meta.nativeAssetType);
        }
        else
        {
            importer = m_impl->create_importer(fileSourcePath.view());
        }

        string_builder workDir;

        if (!m_impl->create_temporary_files_dir(workDir, asset_registry_impl::generate_uuid()))
        {
            return "Failed to register asset"_err;
        }

        if (!importer.init(*m_impl, meta.assetId, workDir, true))
        {
            return "Asset operation failed"_err;
        }

        // TODO: Read previous settings
        data_document settings;

        if (optSettings)
        {
            settings = std::move(*optSettings);
        }

        m_impl->push_import_process(&it->second, std::move(importer), std::move(settings), {}, it->second.assetSource);
        return no_error;
    }

    expected<> asset_registry_impl::create_or_save_asset(asset_entry* optAssetEntry,
        const any_asset& asset,
        uuid assetId,
        cstring_view optSource,
        string_view destination,
        string_view optName,
        h32<asset_repository> assetSource)
    {
        // Find native_asset_descriptor
        const auto it = nativeAssetTypes.find(asset.get_type_uuid());

        if (it == nativeAssetTypes.end())
        {
            return "Failed to register asset"_err;
        }

        const auto& desc = it->second;

        // Create temporary directory

        string_builder workDir;
        if (!create_temporary_files_dir(workDir, generate_uuid()))
        {
            return "Failed to register asset"_err;
        }

        if (!desc.createImporter)
        {
            return "Failed to register asset"_err;
        }

        auto fileImporter = desc.createImporter(desc.userdata);

        if (!fileImporter)
        {
            return "Failed to register asset"_err;
        }

        string_builder sourceFile;
        const bool isReimport = optAssetEntry != nullptr;

        if (optSource.empty())
        {
            sourceFile = workDir;
            sourceFile.append_path(optName);
            sourceFile.append(desc.fileExtension);
        }

        cstring_view source = optSource.empty() ? sourceFile : optSource;

        if (!desc.save(asset, source, workDir, desc.userdata))
        {
            return "Asset operation failed"_err;
        }

        import_config config;
        config.sourceFile = source.as<string>(); // Determine the source file to import

        importer importer(std::move(config), std::move(fileImporter));

        importer.set_native_asset_type(asset.get_type_uuid());

        if (!importer.init(*this, assetId, workDir, isReimport))
        {
            return "Asset not found"_err;
        }

        push_import_process(optAssetEntry, std::move(importer), {}, destination, assetSource);

        return no_error;
    }

    void asset_registry_impl::on_artifact_added(artifact_meta meta, const uuid& processId)
    {
        const auto [it, inserted] = artifactsMap.emplace(meta.artifactId,
            artifact_entry{
                .meta = meta,
                .processId = processId,
            });
        OBLO_ASSERT(inserted);

        if (resourceProvider)
        {
            resourceProvider->push_added_artifact(meta.assetId, meta.artifactId);
        }

        ++versionId;
    }

    void asset_registry_impl::on_artifact_removed(uuid artifactId)
    {
        const auto count = artifactsMap.erase(artifactId);
        OBLO_ASSERT(count > 0);

        if (resourceProvider)
        {
            resourceProvider->push_removed_artifact(artifactId);
        }

        ++versionId;
    }

    void asset_registry_impl::on_artifact_modified(uuid assetId, uuid processId, uuid artifactId)
    {
        const auto it = artifactsMap.find(artifactId);

        if (it != artifactsMap.end())
        {
            it->second.processId = processId;
        }

        if (resourceProvider)
        {
            resourceProvider->push_modified_artifact(assetId, artifactId);
        }

        ++versionId;
    }

    const string_builder* asset_registry_impl::get_asset_filesystem_path(const uuid& id)
    {
        const auto it = assets.find(id);

        if (it == assets.end())
        {
            return nullptr;
        }

        return &it->second.path;
    }

    expected<uuid> asset_registry::create_asset(const any_asset& asset, string_view destination, string_view name)
    {
        OBLO_ASSERT(is_asset_path(destination));

        if (!asset)
        {
            return "Asset operation failed"_err;
        }

        string_builder fullPath;
        const auto assetSource = m_impl->resolve_asset_path(fullPath, destination);

        if (!assetSource)
        {
            return "Failed to register asset"_err;
        }

        const auto assetId = asset_registry_impl::generate_uuid();

        if (!m_impl->create_or_save_asset(nullptr, asset, assetId, {}, destination, name, assetSource))
        {
            return "Failed to register asset"_err;
        }

        return assetId;
    }

    expected<any_asset> asset_registry::load_asset(uuid assetId)
    {
        const auto it = m_impl->assets.find(assetId);

        if (it == m_impl->assets.end())
        {
            return "Asset not found"_err;
        }

        const uuid& type = it->second.meta.nativeAssetType;

        const auto typeIt = m_impl->nativeAssetTypes.find(type);

        if (typeIt == m_impl->nativeAssetTypes.end())
        {
            return "Asset not found"_err;
        }

        string_builder sourceFilePath;

        if (!importer::read_source_file_path(*m_impl, assetId, it->second.assetSource, sourceFilePath))
        {
            return "Failed to load asset"_err;
        }

        any_asset asset;

        if (!typeIt->second.load(asset, sourceFilePath, typeIt->second.userdata))
        {
            return "Failed to load asset"_err;
        }

        return asset;
    }

    expected<> asset_registry::save_asset(const any_asset& asset, uuid assetId)
    {
        const auto it = m_impl->assets.find(assetId);

        if (it == m_impl->assets.end())
        {
            return "Asset not found"_err;
        }

        string_builder sourceFilePath;

        if (!importer::read_source_file_path(*m_impl, assetId, it->second.assetSource, sourceFilePath))
        {
            return "Failed to register asset"_err;
        }

        return m_impl
            ->create_or_save_asset(&it->second, asset, assetId, sourceFilePath, {}, {}, it->second.assetSource);
    }

    unique_ptr<file_importer> asset_registry_impl::create_file_importer(string_view sourceFile) const
    {
        const auto ext = filesystem::extension(sourceFile);

        for (auto& [type, assetImporter] : importers)
        {
            for (const auto& importerExt : assetImporter.extensions)
            {
                if (importerExt == ext)
                {
                    return assetImporter.create(any{});
                }
            }
        }

        return {};
    }

    uuid asset_registry_impl::generate_uuid()
    {
        return uuid_system_generator{}.generate();
    }

    bool asset_registry_impl::create_source_files_dir(
        string_builder& dir, uuid sourceFileId, h32<asset_repository> source)
    {
        return ensure_directories(make_source_files_dir_path(dir, sourceFileId, source));
    }

    string_builder& asset_registry_impl::make_source_files_dir_path(
        string_builder& dir, uuid sourceFileId, h32<asset_repository> source) const
    {
        const auto& assetSource = get_asset_repository(source);
        return dir.clear().append(assetSource.sourceDir).append_path_separator().format("{}", sourceFileId);
    }

    bool asset_registry_impl::create_temporary_files_dir(string_builder& dir, uuid assetId) const
    {
        dir.clear().append(artifactsDir).append_path(".workdir").append_path_separator().format("{}", assetId);
        return ensure_directories(dir);
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

        if (!m_impl->resolve_asset_path(fullPath, path))
        {
            return false;
        }

        fullPath.append(AssetMetaExtension);

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

    bool asset_registry::find_artifact_by_id(const uuid& id, artifact_meta& artifactMeta) const
    {
        const auto it = m_impl->artifactsMap.find(id);
        const bool r = it != m_impl->artifactsMap.end();

        if (r)
        {
            artifactMeta = it->second.meta;
        }

        return r;
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

    const native_asset_descriptor* asset_registry::find_native_asset_type(const uuid& type) const
    {
        const auto it = m_impl->nativeAssetTypes.find(type);
        return it == m_impl->nativeAssetTypes.end() ? nullptr : &it->second;
    }

    void asset_registry::iterate_artifacts_by_type(const uuid& type,
        function_ref<bool(const uuid& assetId, const uuid& artifactId)> callback) const
    {
        artifact_meta meta{};

        for (const auto& [id, asset] : m_impl->assets)
        {
            for (const auto& artifactId : asset.artifacts)
            {
                if (find_artifact_by_id(artifactId, meta) && meta.type == type)
                {
                    if (!callback(id, artifactId))
                    {
                        return;
                    }
                }
            }
        }
    }

    cstring_view asset_registry::resolve_asset_source_path(hashed_string_view id) const
    {
        const auto it = m_impl->assetSourceNameToIdx.find(id);
        OBLO_ASSERT(it != m_impl->assetSourceNameToIdx.end());

        return m_impl->assetRepositories[it->second.value].assetDir;
    }

    void asset_registry::get_asset_source_names(deque<hashed_string_view>& outAssetSources) const
    {
        for (auto& [name, id] : m_impl->assetSourceNameToIdx)
        {
            outAssetSources.emplace_back(name);
        }
    }

    bool asset_registry::resolve_asset_path(string_builder& outBuilder, string_view assetPath) const
    {
        return bool{m_impl->resolve_asset_path(outBuilder, assetPath)};
    }

    bool asset_registry::resolve_asset_meta_path(string_builder& outBuilder, string_view assetPath) const
    {
        if (resolve_asset_path(outBuilder, assetPath))
        {
            outBuilder.append(AssetMetaExtension);
            return true;
        }

        return false;
    }

    bool asset_registry::get_source_directory(const uuid& assetId, string_builder& outPath) const
    {
        const auto it = m_impl->assets.find(assetId);

        if (it == m_impl->assets.end())
        {
            return false;
        }

        m_impl->make_source_files_dir_path(outPath, assetId, it->second.assetSource);
        return true;
    }

    bool asset_registry::get_source_path(const uuid& assetId, string_builder& outPath) const
    {
        const auto it = m_impl->assets.find(assetId);

        if (it == m_impl->assets.end())
        {
            return false;
        }

        m_impl->make_source_files_dir_path(outPath, assetId, it->second.assetSource);

        outPath.append_path_separator();
        return importer::read_source_file_path(*m_impl, assetId, it->second.assetSource, outPath);
    }

    bool asset_registry::get_artifact_path(const uuid& artifactId, string_builder& outPath) const
    {
        const auto it = m_impl->artifactsMap.find(artifactId);

        if (it == m_impl->artifactsMap.end())
        {
            return false;
        }

        m_impl->make_artifact_path(outPath, it->second.meta.assetId, it->second.processId, artifactId);
        return true;
    }

    bool asset_registry::get_asset_name(const uuid& assetId, string_builder& outName) const
    {
        auto* const path = m_impl->get_asset_filesystem_path(assetId);

        if (!path)
        {
            return false;
        }

        outName.append(filesystem::filename(*path));
        return true;
    }

    bool asset_registry::get_asset_path(const uuid& assetId, string_builder& outPath) const
    {
        const auto it = m_impl->assets.find(assetId);

        if (it == m_impl->assets.end())
        {
            return false;
        }

        // Build an asset path from file system path
        auto& source = m_impl->get_asset_repository(it->second.assetSource);
        outPath.append(asset_path_prefix).append(source.name);

        const string_view fsPath = string_view{it->second.path};
        OBLO_ASSERT(fsPath.starts_with(source.assetDir.view()));

        outPath.append_path_separator('/');

        for (char c : fsPath.substr(source.assetDir.size()))
        {
            if constexpr (platform::is_windows())
            {
                if (c == '\\')
                {
                    c = '/';
                }
            }

            if (c == '/' && outPath.view().back() == '/')
            {
                continue;
            }

            outPath.append(c);
        }

        return true;
    }

    bool asset_registry::get_asset_directory_path(const uuid& assetId, string_builder& outPath) const
    {
        if (get_asset_path(assetId, outPath))
        {
            outPath.parent_path();
            return true;
        }

        return false;
    }

    u32 asset_registry::get_running_import_count() const
    {
        return m_impl->currentImports.size32();
    }

    u64 asset_registry::get_version_id() const
    {
        return m_impl->versionId;
    }

    bool asset_registry_impl::on_new_artifact_discovered(
        string_builder& builder, const uuid& artifactId, const uuid& assetId, const uuid& processId)
    {
        artifact_meta artifactMeta;
        make_artifact_path(builder, assetId, processId, artifactId).append(g_artifactMetaExtension);

        if (load_artifact_meta(builder, artifactMeta))
        {
            on_artifact_added(artifactMeta, processId);
            return true;
        }

        return false;
    }

    bool asset_registry_impl::on_new_asset_discovered(
        string_builder& builder, const uuid& assetId, const uuid& processId, deque<uuid>& artifacts)
    {
        bool allSuccessful = true;

        for (auto artifactIt = artifacts.begin(); artifactIt != artifacts.end();)
        {
            const auto artifactId = *artifactIt;

            artifact_meta artifactMeta;
            make_artifact_path(builder, assetId, processId, artifactId).append(g_artifactMetaExtension);

            if (load_artifact_meta(builder, artifactMeta))
            {
                on_artifact_added(artifactMeta, processId);
                ++artifactIt;
            }
            else
            {
                allSuccessful = false;
                artifactIt = artifacts.erase_unordered(artifactIt);
            }
        }

        // We keep artifacts sorted for consistency and easier comparisons
        std::sort(artifacts.begin(), artifacts.end());

        return allSuccessful;
    }

    bool asset_registry_impl::handle_asset_rename(cstring_view newPath)
    {
        OBLO_ASSERT(filesystem::extension(newPath) == AssetMetaExtension);

        asset_meta meta;

        if (!load_asset_meta(meta, newPath))
        {
            return false;
        }

        const auto it = assets.find(meta.assetId);

        if (it == assets.end())
        {
            return false;
        }

        string_builder oldFileName = it->second.path;
        OBLO_ASSERT(filesystem::extension(oldFileName) != AssetMetaExtension);

        oldFileName.append(AssetMetaExtension);

        auto fileIt = assetFileMap.find(oldFileName.view());

        if (fileIt != assetFileMap.end())
        {
            auto n = assetFileMap.extract(fileIt);
            n.key() = newPath;
            n.mapped() = meta.assetId;
            assetFileMap.insert(std::move(n));
        }

        // We keep the meta extension out of the name
        it->second.path = newPath;
        it->second.path.resize(newPath.size() - AssetMetaExtension.size());

        return true;
    }

    void asset_registry::discover_assets(flags<asset_discovery_flags> flags)
    {
        std::error_code ec;

        asset_process_info processInfo;

        string_builder builder;

        deque<uuid> assetsToReprocess;
        usize blockingReprocessCount{};

        // The first entry should just be a dummy
        OBLO_ASSERT(m_impl->assetRepositories.empty() || m_impl->assetRepositories[0].name.empty());

        for (auto& repo : std::span{m_impl->assetRepositories}.subspan(1))
        {
            for (auto&& entry : std::filesystem::recursive_directory_iterator{repo.assetDir.as<std::string>(), ec})
            {
                const auto& p = entry.path();

                if (p.extension() != AssetMetaExtension.c_str() || !is_regular_file(p))
                {
                    continue;
                }

                asset_meta assetMeta{};

                if (load_asset_meta(assetMeta, p))
                {
                    [[maybe_unused]] const auto [fileIt, fileInserted] =
                        m_impl->assetFileMap.emplace(builder.clear().append(p.native().c_str()).as<string>(),
                            assetMeta.assetId);

                    OBLO_ASSERT(fileInserted);

                    processInfo.clear();

                    OBLO_ASSERT(!assetMeta.assetId.is_nil());
                    auto [it, ok] = m_impl->assets.emplace(assetMeta.assetId,
                        asset_entry{
                            .meta = assetMeta,
                            .assetSource = repo.id,
                        });

                    if (!ok)
                    {
                        log::error("An asset id conflict was detected with {}", assetMeta.assetId);
                        continue;
                    }

                    it->second.path.clear().append(p.parent_path().string()).append_path(p.stem().string());

                    bool needsReprocessing = false;

                    m_impl->make_artifacts_process_path(builder.clear(), assetMeta.assetId);

                    if (!load_asset_process_info(builder, processInfo))
                    {
                        log::debug("Failed to load asset process info for {}", assetMeta.assetId);
                        needsReprocessing = true;
                    }
                    else
                    {
                        needsReprocessing = !m_impl->on_new_asset_discovered(builder,
                            assetMeta.assetId,
                            processInfo.processId,
                            processInfo.artifacts);

                        it->second.artifacts.assign(processInfo.artifacts.begin(), processInfo.artifacts.end());
                    }

                    if (needsReprocessing && flags.contains(asset_discovery_flags::reprocess_dirty))
                    {
                        assetsToReprocess.push_back(assetMeta.assetId);

                        if (repo.flags.contains(asset_repository_flags::wait_until_processed))
                        {
                            std::swap(assetsToReprocess[blockingReprocessCount], assetsToReprocess.back());
                            ++blockingReprocessCount;
                        }
                    }
                }
                else
                {
                    log::warn("Failed to load asset meta {}", p.string());
                }
            }
        }

        if (flags.contains(asset_discovery_flags::garbage_collect))
        {
            deque<string_builder> pathsToRemove;

            auto readUuidFromFileName = [](cstring_view filename, uuid& id) { return id.parse_from(filename); };

            for (auto& assetSource : std::span{m_impl->assetRepositories}.subspan(1))
            {
                for (const cstring_view cleanupDir : {assetSource.sourceDir.view(), m_impl->artifactsDir.view()})
                {
                    for (auto&& entry : std::filesystem::directory_iterator{cleanupDir.as<std::string>(), ec})
                    {
                        if (std::filesystem::is_directory(entry.path()) &&
                            entry.path().filename().string().starts_with("."))
                        {
                            pathsToRemove.emplace_back().append(entry.path().c_str());
                            continue;
                        }

                        builder.clear().append(entry.path().filename().stem().c_str());

                        uuid assetId;

                        if (readUuidFromFileName(builder, assetId) && !m_impl->assets.contains(assetId))
                        {
                            pathsToRemove.emplace_back().append(entry.path().c_str());
                        }
                    }
                }
            }

            for (auto& path : pathsToRemove)
            {
                log::debug("Garbage collecting {}", path.c_str());
                filesystem::remove_all(path).assert_value();
            }
        }

        if (blockingReprocessCount > 0)
        {
            for (usize i = 0; i < blockingReprocessCount; ++i)
            {
                const auto& assetId = assetsToReprocess[i];

                if (!process(assetId))
                {
                    log::debug("Failed to reprocess asset {}", assetId);
                }
            }

            // Make sure these are imported first
            while (get_running_import_count() > 0)
            {
                update();
            }
        }

        // Enqueue the processing for non-blocking assets

        for (usize i = blockingReprocessCount; i < assetsToReprocess.size(); ++i)
        {
            const auto& assetId = assetsToReprocess[i];

            if (!process(assetId))
            {
                log::debug("Failed to reprocess asset {}", assetId);
            }
        }
    }

    resource_provider* asset_registry::initialize_resource_provider()
    {
        m_impl->resourceProvider = allocate_unique<artifact_resource_provider>(*m_impl);

        if (!m_impl->artifactsMap.empty())
        {
            for (auto& [id, entry] : m_impl->artifactsMap)
            {
                m_impl->resourceProvider->push_added_artifact(entry.meta.assetId, id);
            }
        }

        return m_impl->resourceProvider.get();
    }

    void asset_registry::update()
    {
        auto* jm = job_manager::get();

        string_builder builder;

        for (auto it = m_impl->currentImports.begin(); it != m_impl->currentImports.end();)
        {
            auto& importProcess = **it;

            if (!jm->try_wait(importProcess.job))
            {
                ++it;
                continue;
            }

            bool success = importProcess.success.load();

            if (!success)
            {
                log::debug("Import execution of {} failed", importProcess.importer.get_config().sourceFile);

                if (importProcess.importer.is_reimport())
                {
                    const auto assetIt = m_impl->assets.find(importProcess.importer.get_asset_id());

                    if (assetIt == m_impl->assets.end())
                    {
                        log::debug("An import execution terminated, but asset {} was not found, maybe it was deleted?",
                            importProcess.importer.get_asset_id());
                    }
                    else
                    {
                        OBLO_ASSERT(assetIt->second.isProcessing);
                        assetIt->second.isProcessing = false;
                    }
                }
            }
            else
            {
                cstring_view destination;

                if (importProcess.importer.is_reimport())
                {
                    const auto assetIt = m_impl->assets.find(importProcess.importer.get_asset_id());

                    if (assetIt == m_impl->assets.end())
                    {
                        log::debug("An import execution terminated, but asset {} was not found, maybe it was deleted?",
                            importProcess.importer.get_asset_id());

                        it = m_impl->currentImports.erase_unordered(it);
                        continue;
                    }

                    filesystem::parent_path(assetIt->second.path.view(), builder);

                    destination = builder.make_canonical_path().as<cstring_view>();
                    importProcess.importer.set_asset_name(filesystem::filename(assetIt->second.path));

                    OBLO_ASSERT(assetIt->second.isProcessing);
                    assetIt->second.isProcessing = false;
                }
                else if (is_asset_path(importProcess.destination))
                {
                    resolve_asset_path(builder, importProcess.destination);
                    destination = builder.as<cstring_view>();
                }
                else
                {
                    destination = importProcess.destination.as<cstring_view>();
                }

                const auto result = importProcess.importer.finalize(*m_impl, destination, importProcess.assetSource);

                if (!result)
                {
                    log::debug("Import finalization of {} failed", importProcess.importer.get_config().sourceFile);
                }

                success = result;
            }

            const auto finishTime = clock::now();

            const f32 executionTime = to_f32_seconds(finishTime - importProcess.startTime);

            log::generic(success ? log::severity::info : log::severity::error,
                "Import of '{}' {}. Execution time: {:.2f}s",
                importProcess.importer.get_config().sourceFile,
                success ? "succeeded" : "failed",
                executionTime);

            it = m_impl->currentImports.erase_unordered(it);
        }

        for (auto& assetSource : std::span{m_impl->assetRepositories}.subspan(1))
        {
            if (!assetSource.watcher)
            {
                continue;
            }

            asset_meta meta;

            assetSource.watcher
                ->process(
                    [this, &builder, &meta, assetSourceId = assetSource.id](
                        const filesystem::directory_watcher_event& e)
                    {
                        switch (e.eventKind)
                        {
                        case filesystem::directory_watcher_event_kind::added:
                            // We should check if the asset is in our map, and add it in case it is not
                            if (e.path.ends_with(AssetMetaExtension) && load_asset_meta(meta, e.path))
                            {
                                const auto probablyInvalidIt = m_impl->assets.find(meta.assetId);

                                [[maybe_unused]] const auto [fileIt, fileInserted] =
                                    m_impl->assetFileMap.emplace(string{e.path.c_str()}, meta.assetId);
                                OBLO_ASSERT(fileInserted);

                                if (probablyInvalidIt == m_impl->assets.end())
                                {
                                    const auto [newAssetIt, inserted] = m_impl->assets.emplace(meta.assetId,
                                        asset_entry{
                                            .assetSource = assetSourceId,
                                        });

                                    auto& newEntry = newAssetIt->second;

                                    m_impl->make_artifacts_process_path(builder.clear(), meta.assetId);

                                    asset_process_info processInfo;

                                    if (!load_asset_process_info(builder, processInfo))
                                    {
                                        log::debug("Failed to load asset process info for {}", meta.assetId);
                                        m_impl->assets.erase(newAssetIt);
                                        m_impl->assetFileMap.erase(fileIt);
                                        return;
                                    }

                                    m_impl->on_new_asset_discovered(builder,
                                        meta.assetId,
                                        processInfo.processId,
                                        processInfo.artifacts);

                                    string_view assetPath{e.path};
                                    assetPath.remove_suffix(AssetMetaExtension.size());

                                    newEntry.path.append(assetPath);
                                    newEntry.meta = meta;
                                    newEntry.artifacts.assign(processInfo.artifacts.begin(),
                                        processInfo.artifacts.end());
                                }
                            }
                            break;

                        case filesystem::directory_watcher_event_kind::removed:
                            // We can remove it from the map, if it's non-atomic rename it should be re-added later
                            if (e.path.ends_with(AssetMetaExtension))
                            {
                                const auto fileIt = m_impl->assetFileMap.find(e.path);

                                if (fileIt != m_impl->assetFileMap.end())
                                {
                                    const auto it = m_impl->assets.find(fileIt->second);

                                    if (it != m_impl->assets.end())
                                    {
                                        for (const auto& artifact : it->second.artifacts)
                                        {
                                            m_impl->on_artifact_removed(artifact);
                                        }

                                        m_impl->assets.erase(it);
                                    }

                                    m_impl->assetFileMap.erase(fileIt);
                                }
                            }

                            break;

                        case filesystem::directory_watcher_event_kind::renamed:
                            // We need to update the asset entry path
                            if (e.path.ends_with(AssetMetaExtension))
                            {
                                [[maybe_unused]] const bool handled = m_impl->handle_asset_rename(e.path);
                                OBLO_ASSERT(handled);
                            }
                            else if (filesystem::is_directory(e.path).value_or(false))
                            {
                                // Recursively handle each asset
                                std::error_code ec;

                                string_builder movedAsset;

                                for (auto&& entry :
                                    std::filesystem::recursive_directory_iterator{e.path.as<std::string>(), ec})
                                {
                                    const auto& p = entry.path();

                                    if (!is_regular_file(p) || p.extension() != AssetMetaExtension.c_str())
                                    {
                                        continue;
                                    }

                                    movedAsset.clear().append(p.c_str());

                                    [[maybe_unused]] const bool handled = m_impl->handle_asset_rename(movedAsset);
                                    OBLO_ASSERT(handled);
                                }
                            }

                            break;

                        case filesystem::directory_watcher_event_kind::modified:
                            // Maybe it was reprocessed
                            if (e.path.ends_with(AssetMetaExtension) && load_asset_meta(meta, e.path))
                            {
                                const auto it = m_impl->assets.find(meta.assetId);
                                const auto fileIt = m_impl->assetFileMap.find(e.path);

                                if (it != m_impl->assets.end() && fileIt != m_impl->assetFileMap.end())
                                {
                                    // We track this asset, check if something has changed
                                    asset_process_info processInfo;

                                    m_impl->make_artifacts_process_path(builder.clear(), meta.assetId);

                                    if (!load_asset_process_info(builder, processInfo))
                                    {
                                        log::debug("Failed to load asset process info for {}", meta.assetId);
                                        return;
                                    }

                                    std::sort(processInfo.artifacts.begin(), processInfo.artifacts.end());

                                    auto& oldEntry = it->second;
                                    OBLO_ASSERT(std::is_sorted(oldEntry.artifacts.begin(), oldEntry.artifacts.end()));

                                    usize oldIndex = 0, newIndex = 0;

                                    while (
                                        oldIndex < oldEntry.artifacts.size() && newIndex < processInfo.artifacts.size())
                                    {
                                        const auto& o = oldEntry.artifacts[oldIndex];
                                        const auto& n = processInfo.artifacts[newIndex];

                                        if (o < n)
                                        {
                                            m_impl->on_artifact_removed(o);
                                            ++oldIndex;
                                        }
                                        else if (o > n)
                                        {
                                            if (!m_impl->on_new_artifact_discovered(builder,
                                                    n,
                                                    meta.assetId,
                                                    processInfo.processId))
                                            {
                                                // Failed to load the artifact, should we remove it?
                                                OBLO_ASSERT(false);
                                            }

                                            ++newIndex;
                                        }
                                        else
                                        {
                                            ++oldIndex;
                                            ++newIndex;

                                            m_impl->on_artifact_modified(meta.assetId, processInfo.processId, o);
                                        }
                                    }

                                    for (; oldIndex < oldEntry.artifacts.size(); ++oldIndex)
                                    {
                                        const auto& o = oldEntry.artifacts[oldIndex];
                                        m_impl->on_artifact_removed(o);
                                    }

                                    for (; newIndex < processInfo.artifacts.size(); ++newIndex)
                                    {
                                        const auto& n = processInfo.artifacts[newIndex];

                                        if (!m_impl->on_new_artifact_discovered(builder,
                                                n,
                                                meta.assetId,
                                                processInfo.processId))
                                        {
                                            // Failed to load the artifact, should we remove it?
                                            OBLO_ASSERT(false);
                                        }
                                    }

                                    oldEntry.artifacts.assign(processInfo.artifacts.begin(),
                                        processInfo.artifacts.end());
                                }
                            }
                            break;

                        default:
                            break;
                        }
                    })
                .assert_value();
        }
    }

    void asset_registry_impl::push_import_process(asset_entry* optEntry,
        importer&& importer,
        data_document&& settings,
        string_view destination,
        h32<asset_repository> assetSource)
    {
        OBLO_ASSERT(assetSource);

        // If the asset is processing we should maybe invalidate the current processing and enqueue a new one
        if (optEntry && optEntry->isProcessing)
        {
            return;
        }

        auto& importProcess = currentImports.emplace_back(allocate_unique<import_process>());

        importProcess->importer = std::move(importer);
        importProcess->settings = std::move(settings);
        importProcess->destination = destination.as<string>();
        importProcess->startTime = clock::now();
        importProcess->assetSource = assetSource;

        auto* jm = job_manager::get();

        const auto job = jm->push_waitable(
            [importProcess = importProcess.get()]
            {
                const auto r = importProcess->importer.execute(importProcess->settings);
                importProcess->success.store(r);
            });

        importProcess->job = job;

        if (optEntry)
        {
            OBLO_ASSERT(!optEntry->isProcessing);
            optEntry->isProcessing = true;
        }
    }

    bool asset_registry_impl::save_artifact(
        const cstring_view srcArtifact, const artifact_meta& meta, const uuid& processId, write_policy policy)
    {
        string_builder artifactPath;
        make_artifact_path(artifactPath, meta.assetId, processId, meta.artifactId);

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
        artifactMetaPath.append(g_artifactMetaExtension);

        if (!filesystem::rename(srcArtifact, artifactPath).value_or(false) ||
            !save_artifact_meta(meta, artifactMetaPath))
        {
            return false;
        }

        const auto srcExtension = filesystem::extension(srcArtifact);

        if (!srcExtension.empty())
        {
            // Create a link with the given extension, to make it easier to look into files
            // This should probably be an option, it might be confusing if tools report the artifacts directory size to
            // be twice as big as it actually is
            auto artifactWithExtension = artifactPath;
            artifactWithExtension.append(srcExtension);

            if (!filesystem::create_hard_link(artifactPath.view(), artifactWithExtension.view()))
            {
                log::debug("Failed to create hard link {}", artifactWithExtension.view());
            }
        }

        return true;
    }

    bool asset_registry_impl::save_asset(string_view destination,
        string_view assetName,
        const asset_meta& meta,
        const uuid& processId,
        const deque<uuid>& artifacts,
        write_policy policy)
    {

        string_builder fullPath;

        make_artifacts_directory_path(fullPath.clear(), meta.assetId, processId);

        if (!ensure_directories(fullPath))
        {
            log::warn("Failed to create artifact directory {}", fullPath);
            return false;
        }

        make_artifacts_process_path(fullPath.clear(), meta.assetId);

        if (!save_asset_process_info(
                asset_process_info{
                    .artifacts = artifacts,
                    .processId = processId,
                },
                fullPath))
        {
            // Not a big issue, it can always be rebuilt, maybe we should log
            log::warn("Saved to file asset process info to {}, re-processing might be required", fullPath);
        }

        if (is_asset_path(destination))
        {
            resolve_asset_path(fullPath.clear(), destination);
        }
        else
        {
            fullPath = destination;
        }

        fullPath.append_path(assetName).append(AssetMetaExtension);

        if (policy == write_policy::no_overwrite && filesystem::exists(fullPath).value_or(true))
        {
            return false;
        }

        if (!save_asset_meta(meta, fullPath))
        {
            return false;
        }

        return true;
    }

    h32<asset_repository> asset_registry_impl::resolve_asset_path(string_builder& out, string_view assetPath) const
    {
        if (!assetPath.starts_with(asset_path_prefix))
        {
            return {};
        }

        string_view assetSourceName = assetPath.substr(1);

        const auto e = assetSourceName.find_first_of("/\\");
        assetSourceName = assetPath.substr(1, e);

        const auto it = assetSourceNameToIdx.find(hashed_string_view{assetSourceName});

        if (it == assetSourceNameToIdx.end())
        {
            return {};
        }

        out = assetRepositories[it->second.value].assetDir;

        out.append(assetPath.substr(1 + assetSourceName.size()));

        return it->second;
    }

    string_builder& asset_registry_impl::make_asset_path(
        string_builder& out, string_view directory, h32<asset_repository> assetSource) const
    {
        OBLO_ASSERT(filesystem::is_relative(directory));

        out.append(get_asset_repository(assetSource).assetDir);
        out.append_path_separator();

        out.append(directory);
        return out;
    }

    string_builder& asset_registry_impl::make_artifact_path(
        string_builder& out, uuid assetId, uuid processId, uuid artifactId) const
    {
        return out.clear()
            .append(artifactsDir)
            .append_path_separator()
            .format("{}", assetId)
            .append_path_separator()
            .format("{}", processId)
            .append_path_separator()
            .format("{}", artifactId);
    }

    string_builder& asset_registry_impl::make_artifacts_process_path(string_builder& out, uuid assetId) const
    {
        return out.append(artifactsDir).append_path_separator().format("{}", assetId).append(g_assetProcessExtension);
    }

    string_builder& asset_registry_impl::make_artifacts_directory_path(
        string_builder& out, uuid assetId, uuid processId) const
    {
        return out.append(artifactsDir)
            .append_path_separator()
            .format("{}", assetId)
            .append_path_separator()
            .format("{}", processId);
    }

    oblo::importer asset_registry_impl::create_importer(string_view sourceFile) const
    {
        const auto ext = filesystem::extension(sourceFile);

        for (auto& [type, assetImporter] : importers)
        {
            for (const auto& importerExt : assetImporter.extensions)
            {
                if (importerExt == ext)
                {
                    return importer{
                        import_config{
                            .sourceFile = sourceFile.as<string>(),
                        },
                        assetImporter.create(any{}),
                    };
                }
            }
        }

        return {};
    }

    const asset_repository& asset_registry_impl::get_asset_repository(h32<asset_repository> source) const
    {
        OBLO_ASSERT(source);
        return assetRepositories[source.value];
    }

    bool asset_registry_impl::create_directories(cstring_view directory)
    {
        return ensure_directories(directory);
    }
}