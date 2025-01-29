#pragma once

#include <oblo/asset/asset_meta.hpp>
#include <oblo/core/deque.hpp>
#include <oblo/core/expected.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/string/transparent_string_hash.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/unique_ptr.hpp>
#include <oblo/core/uuid.hpp>

#include <unordered_map>

namespace oblo
{
    class any_asset;
    class importer;
    class cstring_view;
    class string_view;
    class data_document;

    struct artifact_meta;
    struct asset_meta;
    struct uuid;

    template <typename>
    class deque;

    struct artifact_entry;
    struct asset_entry;
    struct asset_process_info;
    struct file_importer_info;
    struct native_asset_descriptor;
    struct import_process;

    class artifact_resource_provider;

    namespace filesystem
    {
        class directory_watcher;
    }

    class asset_registry_impl
    {
    public:
        enum class write_policy
        {
            no_overwrite,
            overwrite,
        };

    public:
        static uuid generate_uuid();

    public:
        std::unordered_map<type_id, file_importer_info> importers;
        std::unordered_map<uuid, native_asset_descriptor> nativeAssetTypes;
        std::unordered_map<uuid, asset_entry> assets;
        std::unordered_map<uuid, artifact_entry> artifactsMap;

        std::unordered_map<string, uuid, transparent_string_hash, std::equal_to<>> assetFileMap;

        string_builder assetsDir;
        string_builder artifactsDir;
        string_builder sourceFilesDir;

        deque<unique_ptr<import_process>> currentImports;

        unique_ptr<artifact_resource_provider> resourceProvider;
        unique_ptr<filesystem::directory_watcher> watcher;

        u64 versionId{};

    public:
        string_builder& make_asset_path(string_builder& out, string_view directory) const;

        string_builder& make_artifact_path(string_builder& out, uuid assetId, uuid artifactId) const;
        string_builder& make_artifacts_process_path(string_builder& out, uuid assetId) const;
        string_builder& make_artifacts_directory_path(string_builder& out, uuid assetId) const;

        void push_import_process(
            asset_entry* optEntry, importer&& importer, data_document&& settings, string_view destination);

        bool save_artifact(const cstring_view path, const artifact_meta& meta, write_policy policy);

        bool save_asset(string_view destination,
            string_view fileName,
            const asset_meta& meta,
            const deque<uuid>& artifacts,
            write_policy policy);

        [[nodiscard]] unique_ptr<file_importer> create_file_importer(string_view sourceFile) const;

        importer create_importer(string_view sourceFile) const;

        bool create_source_files_dir(string_builder& dir, uuid sourceFileId);
        string_builder& make_source_files_dir_path(string_builder& dir, uuid sourceFileId) const;

        bool create_temporary_files_dir(string_builder& dir, uuid assetId) const;

        bool create_directories(string_view directory);

        expected<> create_or_save_asset(asset_entry* optAssetEntry,
            const any_asset& asset,
            uuid assetId,
            cstring_view optSource,
            string_view destination,
            string_view optName);

        void on_artifact_added(artifact_meta meta);
        void on_artifact_removed(uuid artifactId);

        const string_builder* get_asset_path(const uuid& id);

        bool on_new_artifact_discovered(string_builder& builder, const uuid& artifactId, const uuid& assetId);
        bool on_new_asset_discovered(string_builder& builder, const uuid& assetId, deque<uuid>& artifacts);
    };
}