#pragma once

#include <oblo/asset/import/import_config.hpp>
#include <oblo/asset/import/import_preview.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/string/string.hpp>
#include <oblo/core/string/string_view.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/uuid.hpp>

#include <memory>
#include <span>
#include <unordered_map>

namespace oblo
{
    class asset_registry;
    class importer;
    class import_context;

    struct artifact_meta;
    struct import_artifact;

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

        virtual bool init(const import_config& config, import_preview& preview) = 0;
        virtual bool import(import_context context) = 0;
        virtual file_import_results get_results() = 0;
    };

    class importer
    {
    public:
        struct file_import_data;

    public:
        importer();

        importer(const importer&) = delete;
        importer(importer&&) noexcept;

        importer(import_config config, const type_id& importerType, std::unique_ptr<file_importer> fileImporter);

        ~importer();

        importer& operator=(const importer&) = delete;
        importer& operator=(importer&&) noexcept;

        bool init(asset_registry& registry);

        bool execute(const data_document& importSettings);
        bool finalize(asset_registry& registry, string_view destination);

        bool is_valid() const noexcept;

        const import_config& get_config() const;

    private:
        bool begin_import();
        bool write_source_files(asset_registry& registry, const deque<cstring_view>& sourceFiles);

    private:
        deque<file_import_data> m_fileImports;
        std::unordered_map<uuid, artifact_meta> m_artifacts;
        uuid m_assetId{};
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