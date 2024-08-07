#pragma once

#include <oblo/asset/import_preview.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/string/string.hpp>
#include <oblo/core/string/string_view.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/properties/serialization/data_document.hpp>

#include <memory>
#include <span>
#include <unordered_map>

namespace oblo
{
    class asset_registry;
    class importer;

    struct artifact_meta;
    struct import_artifact;
    struct import_context;

    struct importer_config
    {
        asset_registry* registry;
        string sourceFile;
        data_document settings;
    };

    struct file_import_results
    {
        std::span<import_artifact> artifacts;
        std::span<const string> sourceFiles;
        uuid mainArtifactHint;
    };

    class file_importer
    {
    public:
        virtual ~file_importer() = default;

        virtual bool init(const importer_config& config, import_preview& preview) = 0;
        virtual bool import(const import_context& context) = 0;
        virtual file_import_results get_results() = 0;
    };

    class importer
    {
    public:
        importer();

        importer(const importer&) = delete;
        importer(importer&&) noexcept;

        importer(importer_config config, const type_id& importerType, std::unique_ptr<file_importer> fileImporter);

        ~importer();

        importer& operator=(const importer&) = delete;
        importer& operator=(importer&&) noexcept;

        bool init();

        bool execute(string_view destinationDir, const data_document& importSettings);

        bool is_valid() const noexcept;

        const importer_config& get_config() const;

        uuid get_import_id() const;

    private:
        bool begin_import(asset_registry& registry, std::span<import_node_config> importNodesConfig);
        bool finalize_import(asset_registry& registry, string_view destinationDir);
        bool write_source_files(std::span<const string> sourceFiles);

    private:
        importer_config m_config;
        std::unique_ptr<file_importer> m_importer;
        import_preview m_preview;
        dynamic_array<import_node_config> m_importNodesConfig;
        std::unordered_map<uuid, artifact_meta> m_artifacts;
        uuid m_importId{};
        type_id m_importerType{};
    };

    using create_file_importer_fn = std::unique_ptr<file_importer> (*)();

    struct file_importer_desc
    {
        type_id type;
        create_file_importer_fn create;
        std::span<const string_view> extensions;
    };
}