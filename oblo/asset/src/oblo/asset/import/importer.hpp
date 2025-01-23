#pragma once

#include <oblo/core/deque.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/string/string_view.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/unique_ptr.hpp>
#include <oblo/core/uuid.hpp>

#include <span>
#include <unordered_map>

namespace oblo
{
    class asset_registry;
    class data_document;
    class file_importer;

    struct artifact_meta;
    struct import_config;

    class importer
    {
    public:
        struct file_import_data;

    public:
        static bool read_source_file_path(const asset_registry& registry, uuid sourceFileId, string_builder& out);

    public:
        importer();

        importer(const importer&) = delete;
        importer(importer&&) noexcept;

        importer(import_config config, const type_id& importerType, unique_ptr<file_importer> fileImporter);

        ~importer();

        importer& operator=(const importer&) = delete;
        importer& operator=(importer&&) noexcept;

        bool init(const asset_registry& registry, uuid assetId, cstring_view workDir, bool isReimport);

        bool execute(const data_document& importSettings);
        bool finalize(asset_registry& registry, string_view destination);

        bool is_valid() const noexcept;
        bool is_reimport() const noexcept;

        const import_config& get_config() const;

    private:
        bool begin_import();
        bool write_source_files(asset_registry& registry, const deque<cstring_view>& sourceFiles);

    private:
        deque<file_import_data> m_fileImports;
        string_builder m_temporaryPath;
        std::unordered_map<uuid, artifact_meta> m_artifacts;
        uuid m_assetId{};
        type_id m_importerType{};
        bool m_isReimport{};
    };
}