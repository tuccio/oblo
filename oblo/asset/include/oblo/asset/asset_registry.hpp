#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/flags.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/string/hashed_string_view.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/unique_ptr.hpp>

#include <span>

namespace oblo
{
    class any_asset;
    class asset_registry_impl;
    class asset_writer;
    class data_document;
    class file_importer;
    class resource_provider;
    class string_builder;
    class string;

    template <typename T>
    class deque;

    template <typename T>
    class dynamic_array;

    template <typename T>
    class function_ref;

    struct artifact_meta;
    struct asset_meta;
    struct asset_repository_descriptor;
    struct native_asset_descriptor;
    struct file_importer_descriptor;
    struct uuid;

    enum class asset_discovery_flags
    {
        reprocess_dirty,
        garbage_collect,
        enum_max,
    };

    class asset_registry
    {
    public:
        asset_registry();
        asset_registry(const asset_registry&) = delete;
        asset_registry(asset_registry&&) noexcept = delete;
        asset_registry& operator=(const asset_registry&) = delete;
        asset_registry& operator=(asset_registry&&) noexcept = delete;
        ~asset_registry();

        [[nodiscard]] bool initialize(std::span<const asset_repository_descriptor> assetRepositories,
            cstring_view artifactsDir);

        void shutdown();

        void update();

        void discover_assets(flags<asset_discovery_flags> flags);

        resource_provider* initialize_resource_provider();

        void register_file_importer(const file_importer_descriptor& desc);
        void unregister_file_importer(type_id type);

        void register_native_asset_type(native_asset_descriptor desc);
        void unregister_native_asset_type(uuid type);

        /// @brief Starts an asynchronous import process for a new asset.
        /// This function will look for a suitable importer among the ones registered and start the process.
        /// The asynchronous import will be finalized on the thread calling update, that updates the registry.
        /// @param sourceFile The file to import.
        /// @param destination The asset directory to import into.
        /// @param assetName The name of the asset that will be created.
        /// @param settings The settings for the importer.
        /// @return The uuid of the asset that will be created, or an error if the import failed to start.
        expected<uuid> import(
            string_view sourceFile, string_view destination, string_view assetName, data_document settings);

        /// @brief Triggers an asynchronous processing of a previously created asset.
        /// Imported and native assets will be reprocessed from the stored source files.
        /// @param asset A previously created asset.
        /// @param optSettings Optional settings for the processing, that will replace the previous.
        /// @return An error if processing failed to start.
        expected<> process(uuid asset, data_document* optSettings = nullptr);

        expected<uuid> create_asset(const any_asset& asset, string_view destination, string_view name);

        expected<any_asset> load_asset(uuid assetId);

        expected<> save_asset(const any_asset& asset, uuid assetId);

        bool find_asset_by_id(const uuid& id, asset_meta& assetMeta) const;
        bool find_asset_by_path(cstring_view path, uuid& id, asset_meta& assetMeta) const;
        bool find_asset_by_meta_path(cstring_view path, uuid& id, asset_meta& assetMeta) const;

        bool find_artifact_by_id(const uuid& id, artifact_meta& artifactMeta) const;

        bool find_asset_artifacts(const uuid& id, dynamic_array<uuid>& artifacts) const;

        const native_asset_descriptor* find_native_asset_type(const uuid& type) const;

        void iterate_artifacts_by_type(const uuid& type,
            function_ref<bool(const uuid& assetId, const uuid& artifactId)> callback) const;

        cstring_view resolve_asset_source_path(hashed_string_view sourceId) const;

        void get_asset_source_names(deque<hashed_string_view>& outAssetSources) const;

        bool resolve_asset_path(string_builder& outBuilder, string_view assetPath) const;
        bool resolve_asset_meta_path(string_builder& outBuilder, string_view assetPath) const;

        bool get_source_directory(const uuid& assetId, string_builder& outPath) const;
        bool get_source_path(const uuid& assetId, string_builder& outPath) const;
        bool get_artifact_path(const uuid& artifactId, string_builder& outPath) const;

        bool get_asset_name(const uuid& assetId, string_builder& outName) const;
        bool get_asset_path(const uuid& assetId, string_builder& outPath) const;
        bool get_asset_directory_path(const uuid& assetId, string_builder& outPath) const;

        u32 get_running_import_count() const;

        u64 get_version_id() const;

    private:
        unique_ptr<asset_registry_impl> m_impl;
    };


    inline const cstring_view AssetMetaExtension{".oasset"};
}