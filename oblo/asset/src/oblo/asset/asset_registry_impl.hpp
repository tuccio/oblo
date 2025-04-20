#pragma once

#include <oblo/asset/asset_meta.hpp>
#include <oblo/asset/descriptors/asset_repository_descriptor.hpp>
#include <oblo/core/deque.hpp>
#include <oblo/core/expected.hpp>
#include <oblo/core/handle.hpp>
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
    struct asset_repository;
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

        std::unordered_map<hashed_string_view, h32<asset_repository>, hash<hashed_string_view>> assetSourceNameToIdx;
        dynamic_array<asset_repository> assetRepositories;

        string_builder artifactsDir;

        deque<unique_ptr<import_process>> currentImports;

        unique_ptr<artifact_resource_provider> resourceProvider;

        u64 versionId{};

    public:
        h32<asset_repository> resolve_asset_path(string_builder& out, string_view assetPath) const;

        string_builder& make_asset_path(string_builder& out, string_view directory, h32<asset_repository> source) const;

        string_builder& make_artifact_path(string_builder& out, uuid assetId, uuid processId, uuid artifactId) const;
        string_builder& make_artifacts_process_path(string_builder& out, uuid assetId) const;
        string_builder& make_artifacts_directory_path(string_builder& out, uuid assetId, uuid processId) const;

        void push_import_process(asset_entry* optEntry,
            importer&& importer,
            data_document&& settings,
            string_view destination,
            h32<asset_repository> assetSource);

        bool save_artifact(
            const cstring_view path, const artifact_meta& meta, const uuid& processId, write_policy policy);

        bool save_asset(string_view destination,
            string_view fileName,
            const asset_meta& meta,
            const uuid& processId,
            const deque<uuid>& artifacts,
            write_policy policy);

        [[nodiscard]] unique_ptr<file_importer> create_file_importer(string_view sourceFile) const;

        importer create_importer(string_view sourceFile) const;

        const asset_repository& get_asset_repository(h32<asset_repository> source) const;

        bool create_source_files_dir(string_builder& dir, uuid sourceFileId, h32<asset_repository> source);

        string_builder& make_source_files_dir_path(
            string_builder& dir, uuid sourceFileId, h32<asset_repository> source) const;

        bool create_temporary_files_dir(string_builder& dir, uuid assetId) const;

        bool create_directories(cstring_view directory);

        expected<> create_or_save_asset(asset_entry* optAssetEntry,
            const any_asset& asset,
            uuid assetId,
            cstring_view optSource,
            string_view destination,
            string_view optName,
            h32<asset_repository> assetSource);

        void on_artifact_added(artifact_meta meta, const uuid& processId);
        void on_artifact_removed(uuid artifactId);
        void on_artifact_modified(uuid assetId, uuid processId, uuid artifactId);

        const string_builder* get_asset_filesystem_path(const uuid& id);

        bool on_new_artifact_discovered(
            string_builder& builder, const uuid& artifactId, const uuid& assetId, const uuid& processId);

        bool on_new_asset_discovered(
            string_builder& builder, const uuid& assetId, const uuid& processId, deque<uuid>& artifacts);

        bool handle_asset_rename(cstring_view newPath);
    };

    struct asset_repository
    {
        string_builder assetDir;
        string_builder sourceDir;
        unique_ptr<filesystem::directory_watcher> watcher;
        string name;
        h32<asset_repository> id;
        flags<asset_repository_flags> flags{};
    };
}