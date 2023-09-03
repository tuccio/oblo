#pragma once

#include <oblo/asset/import_preview.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/uuid.hpp>

#include <filesystem>
#include <memory>
#include <span>
#include <string_view>
#include <unordered_map>

namespace oblo::asset
{
    class registry;
    class importer;

    struct artifact_meta;
    struct import_artifact;
    struct import_context;

    struct importer_config
    {
        registry* registry;
        std::filesystem::path sourceFile;
        type_id importer;
    };

    class file_importer
    {
    public:
        virtual ~file_importer() = default;

        virtual bool init(const importer_config& config, import_preview& preview) = 0;
        virtual bool import(const import_context& context) = 0;
    };

    class importer
    {
    public:
        importer();

        importer(const importer&) = delete;
        importer(importer&&) noexcept;

        importer(importer_config config, std::unique_ptr<file_importer> fileImporter);

        ~importer();

        importer& operator=(const importer&) = delete;
        importer& operator=(importer&&) noexcept;

        bool init();

        bool execute(const std::filesystem::path& destinationDir);

        bool is_valid() const noexcept
        {
            return m_importer != nullptr;
        }

        bool add_asset(import_artifact asset, std::span<import_artifact> otherArtifacts);

        void add_source_files(std::span<const std::filesystem::path> sourceFiles);

        const importer_config& get_config() const;

    private:
        struct pending_asset_import;

    private:
        bool begin_import(registry& registry, std::span<import_node_config> importNodesConfig);
        bool finalize_import(registry& registry, const std::filesystem::path& destinationDir);
        bool write_source_files();

    private:
        importer_config m_config;
        std::unique_ptr<file_importer> m_importer;
        import_preview m_preview;
        std::vector<import_node_config> m_importNodesConfig;
        std::vector<pending_asset_import> m_assets;
        std::unordered_map<uuid, artifact_meta> m_artifacts;
        std::vector<std::filesystem::path> m_sourceFiles;
        uuid m_importId{};
    };

    using create_file_importer_fn = std::unique_ptr<file_importer> (*)();

    struct file_importer_desc
    {
        type_id type;
        create_file_importer_fn create;
        std::span<const std::string_view> extensions;
    };
}