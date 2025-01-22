#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/type_id.hpp>

#include <memory>
#include <span>

namespace oblo
{
    class importer;
    class string_builder;
    class string;

    template <typename T>
    class function_ref;

    struct artifact_meta;
    struct asset_meta;
    struct artifact_type_descriptor;
    struct file_importer_desc;
    struct import_preview;
    struct import_node_config;
    struct uuid;

    class asset_registry
    {
    public:
        asset_registry();
        asset_registry(const asset_registry&) = delete;
        asset_registry(asset_registry&&) noexcept = delete;
        asset_registry& operator=(const asset_registry&) = delete;
        asset_registry& operator=(asset_registry&&) noexcept = delete;
        ~asset_registry();

        [[nodiscard]] bool initialize(cstring_view assetsDir, cstring_view artifactsDir, cstring_view sourceFilesDir);

        void shutdown();

        void discover_assets();

        void register_type(const artifact_type_descriptor& desc);
        void unregister_type(type_id type);
        bool has_asset_type(type_id type) const;

        void register_file_importer(const file_importer_desc& desc);
        void unregister_file_importer(type_id type);

        bool create_directories(string_view directory);

        [[nodiscard]] importer create_importer(cstring_view sourceFile);

        bool find_asset_by_id(const uuid& id, asset_meta& assetMeta) const;
        bool find_asset_by_path(cstring_view path, uuid& id, asset_meta& assetMeta) const;
        bool find_asset_by_meta_path(cstring_view path, uuid& id, asset_meta& assetMeta) const;

        bool find_asset_artifacts(const uuid& id, dynamic_array<uuid>& artifacts) const;

        bool load_artifact_meta(const uuid& artifactId, artifact_meta& artifact) const;

        void iterate_artifacts_by_type(type_id type,
            function_ref<bool(const uuid& assetId, const uuid& artifactId)> callback) const;

        cstring_view get_asset_directory() const;

    public:
        static bool find_artifact_resource(
            const uuid& id, type_id& outType, string& outName, string& outPath, const void* userdata);

    private:
        struct impl;
        friend class importer;

        enum class write_policy
        {
            no_overwrite,
            overwrite,
        };

    private:
        uuid generate_uuid();

        bool save_artifact(const uuid& id,
            const type_id& type,
            const void* dataPtr,
            const artifact_meta& meta,
            write_policy policy = write_policy::no_overwrite);

        bool save_asset(string_view destination,
            string_view fileName,
            const asset_meta& meta,
            std::span<const uuid> artifacts,
            write_policy policy = write_policy::no_overwrite);

        bool create_source_files_dir(string_builder& dir, uuid importId);

    private:
        std::unique_ptr<impl> m_impl;
    };

    inline const cstring_view AssetMetaExtension{".oasset"};
}