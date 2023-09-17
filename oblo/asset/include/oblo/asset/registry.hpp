#pragma once

#include <oblo/core/type_id.hpp>

#include <filesystem>
#include <memory>
#include <span>

namespace oblo
{
    struct uuid;
}

namespace oblo::asset
{
    class importer;

    struct artifact_meta;
    struct asset_meta;
    struct type_desc;
    struct file_importer_desc;
    struct import_preview;
    struct import_node_config;

    class registry
    {
    public:
        registry();
        registry(const registry&) = delete;
        registry(registry&&) noexcept = delete;
        registry& operator=(const registry&) = delete;
        registry& operator=(registry&&) noexcept = delete;
        ~registry();

        [[nodiscard]] bool initialize(const std::filesystem::path& assetsDir,
                                      const std::filesystem::path& artifactsDir,
                                      const std::filesystem::path& sourceFilesDir);

        void shutdown();

        void register_type(const type_desc& desc);
        void unregister_type(type_id type);
        bool has_asset_type(type_id type) const;

        void register_file_importer(const file_importer_desc& desc);
        void unregister_file_importer(type_id type);

        bool create_directories(const std::filesystem::path& directory);

        [[nodiscard]] importer create_importer(const std::filesystem::path& sourceFile);

        bool find_asset_by_path(const std::filesystem::path& path, uuid& id, asset_meta& assetMeta) const;

        const std::filesystem::path& get_asset_directory() const;

    public:
        static bool find_artifact_resource(const uuid& id,
                                           type_id& type,
                                           std::filesystem::path& path,
                                           const void* userdata);

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

        bool save_asset(const std::filesystem::path& destination,
                        std::string_view fileNameBuffer,
                        const asset_meta& meta,
                        write_policy policy = write_policy::no_overwrite);

        std::filesystem::path create_source_files_dir(uuid importId);

    private:
        std::unique_ptr<impl> m_impl;
    };

    constexpr std::filesystem::path::string_type AssetMetaExtension{L".oasset"};
}