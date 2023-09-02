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
    class any_asset;
    class importer;
    struct asset_meta;
    struct asset_type_desc;
    struct file_importer_desc;
    struct import_preview;
    struct import_node_config;

    struct import_artifact;

    class asset_registry
    {
    public:
        asset_registry();
        asset_registry(const asset_registry&) = delete;
        asset_registry(asset_registry&&) noexcept = delete;
        asset_registry& operator=(const asset_registry&) = delete;
        asset_registry& operator=(asset_registry&&) noexcept = delete;
        ~asset_registry();

        [[nodiscard]] bool initialize(const std::filesystem::path& assetsDir,
                                      const std::filesystem::path& artifactsDir);

        void shutdown();

        void register_type(const asset_type_desc& desc);
        void register_file_importer(const file_importer_desc& desc);

        [[nodiscard]] importer create_importer(const std::filesystem::path& sourceFile);

        bool add_imported_asset(uuid importUuid, import_artifact asset, std::span<import_artifact> otherArtifacts);

        const asset_meta& find_asset_by_path(const std::filesystem::path& path) const;

    private:
        struct impl;
        friend class importer;

    private:
        uuid begin_import(const import_preview& preview, std::span<import_node_config> importNodesConfig);
        bool finalize_import(const uuid& importUuid, const std::filesystem::path& destinationDir);

    private:
        std::unique_ptr<impl> m_impl;
    };
}