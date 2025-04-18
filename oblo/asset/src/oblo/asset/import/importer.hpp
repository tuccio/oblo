#pragma once

#include <oblo/core/deque.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/string/string_view.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/unique_ptr.hpp>
#include <oblo/core/uuid.hpp>

#include <span>
#include <unordered_map>

namespace oblo
{
    class asset_registry_impl;
    class data_document;
    class file_importer;

    struct artifact_meta;
    struct asset_source;
    struct import_config;

    class importer
    {
    public:
        struct file_import_data;

    public:
        static bool read_source_file_path(
            const asset_registry_impl& registry, uuid sourceFileId, h32<asset_source> assetSource, string_builder& out);

    public:
        importer();

        importer(const importer&) = delete;
        importer(importer&&) noexcept;

        importer(import_config config, unique_ptr<file_importer> fileImporter);

        ~importer();

        importer& operator=(const importer&) = delete;
        importer& operator=(importer&&) noexcept;

        bool init(const asset_registry_impl& registry, uuid assetId, cstring_view workDir, bool isReimport);

        void set_native_asset_type(uuid nativeAssetType);
        void set_asset_name(string_view assetName);

        bool execute(const data_document& importSettings);
        bool finalize(asset_registry_impl& registry, cstring_view destination, h32<asset_source> assetSource);

        bool is_valid() const noexcept;
        bool is_reimport() const noexcept;

        const import_config& get_config() const;

        uuid get_asset_id() const;

    private:
        bool begin_import();
        bool write_source_files(
            asset_registry_impl& registry, const deque<cstring_view>& sourceFiles, h32<asset_source> assetSource);

    private:
        deque<file_import_data> m_fileImports;
        string_builder m_temporaryPath;
        string m_assetName;
        std::unordered_map<uuid, artifact_meta> m_artifacts;
        uuid m_assetId{};
        uuid m_nativeAssetType{};
        bool m_isReimport{};
    };
}